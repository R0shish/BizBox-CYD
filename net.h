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

enum NetEvtType : uint8_t { EV_PAYMENT = 0 };

struct NetEvent {
  uint8_t type;
  bool stale;           // EV_PAYMENT: book it, but don't announce
  uint32_t paisa;
  char note[32];
  char title[48];
};

QueueHandle_t netQueue = nullptr;
SemaphoreHandle_t cfgLock = nullptr;
volatile bool netPollNow = false;          // set by the loop: "poll as soon as you can"
volatile uint8_t netDot = NET_OK;          // what the status dot should show

static portMUX_TYPE netStatusMux = portMUX_INITIALIZER_UNLOCKED;
static char netStatusBuf[96] = "Idle";

void netSetStatus(const String& s) {
  portENTER_CRITICAL(&netStatusMux);
  strncpy(netStatusBuf, s.c_str(), sizeof netStatusBuf - 1);
  netStatusBuf[sizeof netStatusBuf - 1] = 0;
  portEXIT_CRITICAL(&netStatusMux);
}

String netStatus() {
  char b[sizeof netStatusBuf];
  portENTER_CRITICAL(&netStatusMux);
  memcpy(b, netStatusBuf, sizeof b);
  portEXIT_CRITICAL(&netStatusMux);
  return String(b);
}

struct CfgLock {
  CfgLock() { if (cfgLock) xSemaphoreTake(cfgLock, portMAX_DELAY); }
  ~CfgLock() { if (cfgLock) xSemaphoreGive(cfgLock); }
};

static void netCopy(char* dst, size_t n, const String& s) {
  String c = utf8Cut(s, n - 1);
  strncpy(dst, c.c_str(), n - 1);
  dst[n - 1] = 0;
}

static bool netPostPayment(uint32_t paisa, const String& note, const String& title, bool stale) {
  NetEvent e;
  memset(&e, 0, sizeof e);
  e.type = EV_PAYMENT;
  e.paisa = paisa;
  e.stale = stale;
  netCopy(e.note, sizeof e.note, note);
  netCopy(e.title, sizeof e.title, title);
  return xQueueSend(netQueue, &e, pdMS_TO_TICKS(2000)) == pdTRUE;
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
    netPostPayment(r.pays[i].paisa, r.pays[i].note, r.pays[i].title, r.pays[i].stale);
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

String netSince = "0";

static bool netPollAppsScript(const PollCfg& c) {
  if (c.url.isEmpty()) { netSetStatus("Apps Script URL missing"); return false; }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  String u = c.url + "?key=" + urlEnc(c.key) + "&since=" + netSince;
  if (c.imap.query.length()) u += "&q=" + urlEnc(c.imap.query);
  bool ok = false;
  if (http.begin(client, u)) {
    int code = http.GET();
    if (code == 200) {
      JsonDocument doc;
      if (!deserializeJson(doc, http.getString()) && !doc["error"].is<const char*>()) {
        ok = true;
        netSince = doc["latest"].as<String>();
        int n = 0;
        for (JsonObject p : doc["payments"].as<JsonArray>()) {
          uint32_t a = 0;
          if (!moneyFromDouble(p["amount"].as<double>(), a)) continue;
          String note = p["from"].as<String>();
          netPostPayment(a, note, note.length() ? note : String("eSewa Pay"), false);
          n++;
        }
        netSetStatus("Apps Script OK - " + String(n) + " new");
      } else netSetStatus("Apps Script sent bad JSON");
    } else netSetStatus("Apps Script HTTP " + String(code));
    http.end();
  }
  return ok;
}

// ---------- The task ----------
const uint32_t NET_POLL_MS = 15000;

void netTask(void*) {
  uint32_t lastPoll = 0;
  bool first = true, more = false;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(200));
    if (WiFi.status() != WL_CONNECTED) continue;
    uint32_t now = millis();
    bool due = first || netPollNow || now - lastPoll >= (more ? 1000UL : NET_POLL_MS);
    if (!due) continue;
    first = false;
    netPollNow = false;
    lastPoll = now;

    PollCfg c;
    {
      CfgLock l;
      c.mode = cfg.emailMode;
      c.imap.user = cfg.gmailUser;
      c.imap.pass = cfg.gmailPass;
      c.imap.query = cfg.imapQuery;
      c.imap.currency = cfg.currency;
      c.imap.verifyTls = cfg.verifyGoogle;
      c.url = cfg.url;
      c.key = cfg.key;
    }
    if (c.mode == 2) { netDot = NET_OK; more = false; continue; }   // local API only
    netDot = NET_SYNC;
    bool ok = c.mode == 0 ? netPollGmail(c, more) : netPollAppsScript(c);
    if (c.mode != 0) more = false;
    netDot = ok ? NET_OK : NET_ERR;
  }
}

void netBegin(const ImapState& st) {
  netImapState = st;
  netQueue = xQueueCreate(12, sizeof(NetEvent));
  cfgLock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
}
