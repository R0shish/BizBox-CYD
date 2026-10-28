#pragma once
// ==========================================================================
//  Network task: everything slow (TLS, IMAP, HTTP) runs here, on core 0,
//  so the screen, touch, web dashboard and BOOT button never freeze.
//
//  Rules that keep it safe:
//   - Only the Arduino loop task touches the TFT, audio, orders and stats.
//     This task sends it NetEvents through a queue.
//   - This task never reads `cfg` directly: it copies what it needs under
//     cfgLock, and the loop takes cfgLock while it changes those fields.
//   - Status text for the dashboard goes through netSetStatus/netStatus.
// ==========================================================================

enum NetEvtType : uint8_t {
  EV_PAYMENT = 0,   // from Gmail / Apps Script: book it, announce it
  EV_ORDER,         // Connect pull: add/update/remove an order by external ID
  EV_SAY,           // Connect: speak text
  EV_ANNOUNCE,      // Connect (signed): a payment to announce and book
  EV_SCREEN,        // Connect: switch screen (status = screen index)
};
enum : uint8_t { SRC_GMAIL = 0, SRC_APPS_SCRIPT };
enum : uint8_t { HAS_TITLE = 1, HAS_AMOUNT = 2, HAS_STATUS = 4 };

struct NetEvent {
  uint8_t type;
  uint8_t source;       // EV_PAYMENT: SRC_*
  uint8_t has;          // EV_ORDER: which fields the system sent (HAS_*)
  uint8_t status;       // EV_ORDER: 0-3 = NEW..PAID, 4 = remove; EV_SCREEN: screen
  bool stale;           // EV_PAYMENT: book it, but don't announce
  uint32_t paisa;
  char note[32];        // payment note / sender
  char ext[40];         // EV_ORDER: the system's order ID
  char text[100];       // order title, or text to say
};

QueueHandle_t netQueue = nullptr;
SemaphoreHandle_t cfgLock = nullptr;
volatile bool netPollNow = false;          // set by the loop: "poll as soon as you can"
volatile uint32_t cfgVersion = 1;          // bumped (under cfgLock) whenever the settings change
volatile uint8_t netDot = NET_OK;          // what the status dot should show

// A line of status text written by the network task, read by the web pages
class StatusText {
 public:
  explicit StatusText(const char* init) { strncpy(buf_, init, sizeof buf_ - 1); }
  void set(const String& s) {
    portENTER_CRITICAL(&mux_);
    strncpy(buf_, s.c_str(), sizeof buf_ - 1);
    buf_[sizeof buf_ - 1] = 0;
    portEXIT_CRITICAL(&mux_);
  }
  String get() {
    char b[sizeof buf_];
    portENTER_CRITICAL(&mux_);
    memcpy(b, buf_, sizeof b);
    portEXIT_CRITICAL(&mux_);
    return String(b);
  }
 private:
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  char buf_[96] = {0};
};

StatusText payStatus("Idle");     // payment source (Gmail / Apps Script)
void netSetStatus(const String& s) { payStatus.set(s); }
String netStatus() { return payStatus.get(); }

struct CfgLock {
  CfgLock() { if (cfgLock) xSemaphoreTake(cfgLock, portMAX_DELAY); }
  ~CfgLock() { if (cfgLock) xSemaphoreGive(cfgLock); }
};

static void netCopy(char* dst, size_t n, const String& s) {
  String c = utf8Cut(s, n - 1);
  strncpy(dst, c.c_str(), n - 1);
  dst[n - 1] = 0;
}

static bool netPostPayment(uint8_t source, uint32_t paisa, const String& note, const String& title, bool stale) {
  NetEvent e;
  memset(&e, 0, sizeof e);
  e.type = EV_PAYMENT;
  e.source = source;
  e.paisa = paisa;
  e.stale = stale;
  netCopy(e.note, sizeof e.note, note);
  netCopy(e.text, sizeof e.text, title);
  // The mail is already marked read, so wait for the loop rather than drop it
  // (it only stalls while an announcement plays; the queue holds 2 batches)
  return xQueueSend(netQueue, &e, pdMS_TO_TICKS(30000)) == pdTRUE;
}

// ---------- Payment sources ----------
struct PollCfg {
  int mode;
  ImapCfg imap;
  String url, key;
};

ImapState netImapState = {0, 0};   // owned by the net task after start

static bool netPollGmail(const PollCfg& c, bool& more) {
  ImapResult r;
  ImapState st = netImapState;
  bool ok = imapPoll(c.imap, st, r, (uint32_t)time(nullptr));
  for (int i = 0; i < r.count; i++)
    netPostPayment(SRC_GMAIL, r.pays[i].paisa, r.pays[i].note, r.pays[i].title, r.pays[i].stale);
  // Persist the mailbox position only after the payments are queued
  if (st.uidValidity != netImapState.uidValidity || st.lastUid != netImapState.lastUid) {
    netImapState = st;
    Preferences p;
    p.begin("cydbiz", false);
    p.putULong("guidv", st.uidValidity);
    p.putULong("guid", st.lastUid);
    p.end();
  }
  netSetStatus(r.status);
  more = r.more;
  return ok;
}

String netSince = "0";   // Apps Script cursor (NVS "since"), owned by the net task

static bool netPollAppsScript(const PollCfg& c) {
  if (c.url.isEmpty()) { netSetStatus("Apps Script URL missing"); return false; }
  WiFiClientSecure client;
  if (c.imap.verifyTls) client.setCACert(GOOGLE_ROOTS_PEM);   // script.google.com is on GTS too
  else client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  String u = c.url + (c.url.indexOf('?') >= 0 ? "&" : "?") + "key=" + urlEnc(c.key) + "&since=" + urlEnc(netSince);
  if (c.imap.query.length()) u += "&q=" + urlEnc(c.imap.query);
  bool ok = false;
  if (http.begin(client, u)) {
    int code = http.GET();
    if (code == 200) {
      JsonDocument doc;
      if (!deserializeJson(doc, http.getString()) && !doc["error"].is<const char*>()) {
        ok = true;
        JsonVariant latest = doc["latest"];
        if (!latest.isNull()) {
          String ns = latest.as<String>();
          if (ns.length() && ns != netSince) {
            netSince = ns;
            Preferences p;
            p.begin("cydbiz", false);
            p.putString("since", ns);
            p.end();
          }
        }
        int n = 0;
        for (JsonObject p : doc["payments"].as<JsonArray>()) {
          uint32_t a = 0;
          if (!moneyFromDouble(p["amount"].as<double>(), a)) continue;
          String note = p["from"].as<String>();
          netPostPayment(SRC_APPS_SCRIPT, a, note, note.length() ? note : String("eSewa Pay"), false);
          n++;
        }
        netSetStatus("Apps Script OK - " + String(n) + " new");
      } else netSetStatus("Apps Script sent bad JSON");
    } else netSetStatus("Apps Script HTTP " + String(code));
    http.end();
  }
  return ok;
}

