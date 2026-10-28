#pragma once
// ==========================================================================
//  Connect: webhooks out, polling in. Design and payloads: docs/CONNECT.md
//
//  Loop task:     connectEmit() renders an event and queues it.
//  Network task:  connectDeliverDue() sends queued webhooks (with retries),
//                 connectPull() fetches orders/commands and posts them to the
//                 loop as NetEvents. Neither ever touches the screen.
//  The webhook queue is shared and guarded by cxLock.
// ==========================================================================

const int CX_QUEUE = 8;
const int CX_MAX_ORDERS = 20;       // per pull
const int CX_MAX_COMMANDS = 10;     // per pull
// ---------- Status lines for the dashboard ----------
StatusText cxHookStatus("No webhook sent yet");
StatusText cxPullStatus("Not pulled yet");
StatusText cxCursorText("");

// ---------- HMAC-SHA256 ----------
String hmacSha256Hex(const String& key, const String& msg) {
  uint8_t out[32];
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const unsigned char*)key.c_str(), key.length(),
                      (const unsigned char*)msg.c_str(), msg.length(), out) != 0) return "";
  return toHex(out, 32);
}

// Constant-time compare (signatures)
bool sameSecret(const String& a, const String& b) {
  if (a.length() != b.length()) return false;
  uint8_t d = 0;
  for (unsigned i = 0; i < a.length(); i++) d |= (uint8_t)(a[i] ^ b[i]);
  return d == 0;
}

// ---------- Outbound: queue (loop side) ----------
struct CxHook {
  bool used;
  uint32_t seq;       // queue order: the smallest is the oldest
  String event, id, body;
  uint32_t ts;        // event time (unix), 0 if the clock wasn't set
  uint8_t tries;
  uint32_t dueAt;     // millis()
};
CxHook cxQueue[CX_QUEUE];
SemaphoreHandle_t cxLock = nullptr;
uint32_t cxSeq = 0;
String cxBootId;     // random per boot, part of every delivery ID
uint32_t cxDropped = 0;

struct CxLockGuard {
  CxLockGuard() { if (cxLock) xSemaphoreTake(cxLock, portMAX_DELAY); }
  ~CxLockGuard() { if (cxLock) xSemaphoreGive(cxLock); }
};

int connectQueued() {
  CxLockGuard l;
  int n = 0;
  for (int i = 0; i < CX_QUEUE; i++) n += cxQueue[i].used;
  return n;
}

// What an event is about. Unused fields stay empty / 0.
struct ConnectData {
  const char* source = "";
  uint32_t paisa = 0;
  String note;
  int32_t orderId = 0;
  String ext, title, status, oldStatus;
  bool test = false;
};

static String cxAmount(uint32_t paisa) {
  String s = String(paisa / 100);
  if (paisa % 100) { s += '.'; s += (char)('0' + (paisa % 100) / 10); if (paisa % 10) s += (char)('0' + paisa % 10); }
  return s;
}

static String cxLocalIso(uint32_t ts) {
  if (!ts) return "";
  time_t t = (time_t)ts;
  struct tm tmv;
  localtime_r(&t, &tmv);
  char b[32];
  strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%S", &tmv);
  return String(b) + "+05:45";    // the clock runs on Nepal time (wifi.h)
}

// Queues one event for the webhook URL (loop task). Returns false if no
// webhook is configured or the event type is switched off.
bool connectEmit(const char* event, const ConnectData& d) {
  if (cfg.cxHook.isEmpty()) return false;
  uint8_t bit = !strcmp(event, "payment.received") ? CX_EVT_PAYMENT : !strcmp(event, "order.created") ? CX_EVT_ORDER : CX_EVT_STATUS;
  if (!d.test && !(cfg.cxEvents & bit)) return false;

  time_t now = time(nullptr);
  uint32_t ts = now > 1600000000 ? (uint32_t)now : 0;
  String id = "evt_" + cxBootId + "_" + String(++cxSeq);
  String vals[] = {event, id, String(ts), cxLocalIso(ts), MDNS_NAME, jsonEsc(shopName()), d.test ? "true" : "false",
                   cxAmount(d.paisa), String(d.paisa), jsonEsc(cfg.currency), jsonEsc(d.note), d.source,
                   String(d.orderId), jsonEsc(d.ext), jsonEsc(d.title), d.status, d.oldStatus, ""};
  static const char* const KEYS[] = {"event", "id", "ts", "time", "device", "shop", "test", "amount", "amount_paisa",
                                     "currency", "note", "source", "order_id", "ext_id", "title", "status", "old_status", "body"};
  const int N = sizeof(KEYS) / sizeof(KEYS[0]);
  String body = renderTemplate(
      "{\"event\":\"{{event}}\",\"id\":\"{{id}}\",\"ts\":{{ts}},\"device\":\"{{device}}\",\"shop\":\"{{shop}}\","
      "\"test\":{{test}},\"data\":{\"amount\":{{amount}},\"amount_paisa\":{{amount_paisa}},\"currency\":\"{{currency}}\","
      "\"note\":\"{{note}}\",\"source\":\"{{source}}\",\"order_id\":{{order_id}},\"ext_id\":\"{{ext_id}}\","
      "\"title\":\"{{title}}\",\"status\":\"{{status}}\",\"old_status\":\"{{old_status}}\"}}",
      KEYS, vals, N);
  if (cfg.cxTpl.length()) {
    vals[N - 1] = body;
    body = renderTemplate(cfg.cxTpl, KEYS, vals, N);
  }

  CxLockGuard l;
  int slot = -1, oldest = 0;
  for (int i = 0; i < CX_QUEUE; i++) {
    if (!cxQueue[i].used) { slot = i; break; }
    if (cxQueue[i].seq < cxQueue[oldest].seq) oldest = i;
  }
  if (slot < 0) { slot = oldest; cxDropped++; }   // full: the oldest waiting event goes
  CxHook& h = cxQueue[slot];
  h.used = true;
  h.seq = cxSeq;
  h.event = event;
  h.id = id;
  h.body = body;
  h.ts = ts;
  h.tries = 0;
  h.dueAt = millis();
  return true;
}

// ---------- HTTP (network task) ----------
struct CxCfg {          // what the network task needs, copied under cfgLock
  String hook, header, secret;
  String pull, pOrders, pId, pTitle, pAmount, pStatus, pCommands, pCursor, statusMap;
  uint32_t everyMs;
  bool requireSigned;
};

// "Name: value" -> addHeader (CR/LF never reach the wire)
static void cxAddCustomHeader(HTTPClient& http, const String& line) {
  int c = line.indexOf(':');
  if (c <= 0 || line.indexOf('\r') >= 0 || line.indexOf('\n') >= 0) return;
  String n = line.substring(0, c), v = line.substring(c + 1);
  n.trim(); v.trim();
  if (n.length() && v.length()) http.addHeader(n, v);
}

static bool cxBegin(HTTPClient& http, WiFiClient& plain, WiFiClientSecure& tls, const String& url) {
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (url.startsWith("https://")) {
    tls.setInsecure();     // arbitrary hosts: no CA store; integrity comes from the HMAC (docs/CONNECT.md)
    return http.begin(tls, url);
  }
  return url.startsWith("http://") && http.begin(plain, url);
}

static String cxClock() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return "";
  char b[12];
  snprintf(b, sizeof b, " %02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  return b;
}

// Sends the next due webhook, if any. Returns true if it tried one.
bool connectDeliverDue(const CxCfg& c) {
  static const uint32_t BACKOFF_MS[] = {10000, 60000, 300000, 900000};
  int slot = -1;
  String event, id, body;
  uint32_t ts = 0;
  {
    CxLockGuard l;
    uint32_t now = millis();
    for (int i = 0; i < CX_QUEUE; i++)
      if (cxQueue[i].used && (int32_t)(now - cxQueue[i].dueAt) >= 0) { slot = i; break; }
    if (slot < 0) return false;
    if (c.hook.isEmpty()) { cxQueue[slot].used = false; cxQueue[slot].body = String(); return false; }   // URL removed
    event = cxQueue[slot].event; id = cxQueue[slot].id; body = cxQueue[slot].body; ts = cxQueue[slot].ts;
  }

  WiFiClient plain;
  WiFiClientSecure tls;
  HTTPClient http;
  int code = -1;
  if (cxBegin(http, plain, tls, c.hook)) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "BizBox/" FW_VERSION);
    http.addHeader("X-BizBox-Event", event);
    http.addHeader("X-BizBox-Delivery", id);
    http.addHeader("X-BizBox-Timestamp", String(ts));
    if (c.secret.length()) http.addHeader("X-BizBox-Signature", "sha256=" + hmacSha256Hex(c.secret, String(ts) + "." + body));
    cxAddCustomHeader(http, c.header);
    code = http.POST(body);
    http.end();
  }

  bool ok = code >= 200 && code < 300;
  bool retry = code < 0 || code == 408 || code == 429 || code >= 500;
  String what = code < 0 ? String("no connection") : "HTTP " + String(code);
  {
    CxLockGuard l;
    CxHook& h = cxQueue[slot];
    if (h.used && h.id == id) {                     // still ours (not replaced while we sent it)
      if (ok || !retry || h.tries >= 4) { h.used = false; h.body = String(); }   // free the body now
      else { h.dueAt = millis() + BACKOFF_MS[h.tries]; h.tries++; }
      if (!ok && retry && h.used) what += ", retry " + String(h.tries) + "/4";
      else if (!ok) what += ", dropped";
    }
  }
  cxHookStatus.set(what + " · " + event + cxClock());
  return true;
}

// ---------- Inbound pull (network task) ----------
// Dot path into a JSON value: "data.items", "lines.0.total", "" = itself
JsonVariantConst jsonAt(JsonVariantConst v, const String& path) {
  int pos = 0, len = path.length();
  while (pos < len && !v.isNull()) {
    int dot = path.indexOf('.', pos);
    if (dot < 0) dot = len;
    String seg = path.substring(pos, dot);
    pos = dot + 1;
    if (!seg.length()) continue;
    bool num = true;
    for (unsigned i = 0; i < seg.length(); i++) num &= isdigit((uint8_t)seg[i]) != 0;
    if (num && v.is<JsonArrayConst>()) v = v[(size_t)seg.toInt()];
    else v = v[seg.c_str()];
  }
  return v;
}

// Rupees from JSON: 450, 499.5 or "1,200.50"; 0 is allowed here (an order without a price)
static bool cxMoney(JsonVariantConst v, uint32_t& paisa) {
  if (v.is<const char*>()) {
    String s = v.as<const char*>();
    s.trim();
    if (s == "0" || s == "0.00") { paisa = 0; return true; }
    return parseMoney(s, paisa);
  }
  if (v.is<double>() || v.is<long>()) {
    double d = v.as<double>();
    if (d == 0) { paisa = 0; return true; }
    return moneyFromDouble(d, paisa);
  }
  return false;
}

String cxCursor;                       // last cursor (NVS cxCur), owned by the net task
volatile bool cxCursorReset = false;   // set by a dashboard Save
volatile bool cxPullNow = false;       // "Test pull"
static uint32_t cxSeenCmd[32];         // FNV hashes of the last command IDs
static int cxSeenNext = 0;

static bool cxSeen(uint32_t h) {
  for (int i = 0; i < 32; i++) if (cxSeenCmd[i] == h) return true;
  return false;
}

// Turns one pull reply into NetEvents. Returns false if the loop's queue
// stayed full, so the cursor isn't advanced and the reply is re-read later.
// post() is netPost in the firmware and a recorder in the tests.
struct CxPullStats { int orders = 0, commands = 0, skipped = 0; };
bool connectApplyPull(JsonDocument& doc, const CxCfg& c, bool signedReply, bool (*post)(const NetEvent&), CxPullStats& st) {
  JsonVariantConst root = doc.as<JsonVariantConst>();
  JsonArrayConst list = jsonAt(root, c.pOrders).as<JsonArrayConst>();
  for (JsonVariantConst it : list) {
    if (st.orders >= CX_MAX_ORDERS) break;
    String ext = jsonAt(it, c.pId).as<String>();
    if (!ext.length() || ext == "null") { st.skipped++; continue; }
    NetEvent e;
    memset(&e, 0, sizeof e);
    e.type = EV_ORDER;
    netCopy(e.ext, sizeof e.ext, ext);
    JsonVariantConst t = jsonAt(it, c.pTitle);
    if (!t.isNull()) { e.has |= HAS_TITLE; netCopy(e.text, sizeof e.text, t.as<String>()); }
    uint32_t p = 0;
    if (cxMoney(jsonAt(it, c.pAmount), p)) { e.has |= HAS_AMOUNT; e.paisa = p; }
    JsonVariantConst s = jsonAt(it, c.pStatus);
    if (!s.isNull()) {
      int m = mapStatus(c.statusMap, s.as<String>());
      if (m >= 0) { e.has |= HAS_STATUS; e.status = (uint8_t)m; }
    }
    if (!post(e)) return false;
    st.orders++;
  }
  JsonArrayConst cmds = jsonAt(root, c.pCommands).as<JsonArrayConst>();
  for (JsonVariantConst cmd : cmds) {
    if (st.commands >= CX_MAX_COMMANDS) break;
    String id = cmd["id"].as<String>();
    uint32_t h = id.length() && id != "null" ? fnv1a(id) : 0;
    if (h && cxSeen(h)) { st.skipped++; continue; }
    String type = cmd["type"].as<String>();
    NetEvent e;
    memset(&e, 0, sizeof e);
    if (type == "say") {
      e.type = EV_SAY;
      netCopy(e.text, sizeof e.text, asciiOnly(cmd["text"].as<String>(), 120));
      if (!e.text[0]) { st.skipped++; continue; }
    } else if (type == "screen") {
      String sc = cmd["screen"].as<String>();
      sc.toLowerCase();
      static const char* const NAMES[5] = {"pay", "orders", "board", "stats", "setup"};
      int n = -1;
      for (int i = 0; i < 5; i++) if (sc == NAMES[i] || sc == String(i + 1)) n = i;
      if (n < 0) { st.skipped++; continue; }
      e.type = EV_SCREEN;
      e.status = (uint8_t)n;
    } else if (type == "announce") {
      uint32_t p = 0;
      if (!signedReply || !cxMoney(cmd["amount"], p) || !p) { st.skipped++; continue; }   // money needs a signature
      e.type = EV_ANNOUNCE;
      e.paisa = p;
      netCopy(e.note, sizeof e.note, cmd["note"].isNull() ? String("Payment") : cmd["note"].as<String>());
    } else { st.skipped++; continue; }
    if (!post(e)) return false;
    if (h) { cxSeenCmd[cxSeenNext] = h; cxSeenNext = (cxSeenNext + 1) % 32; }
    st.commands++;
  }
  return true;
}

static bool cxPostToLoop(const NetEvent& e) { return xQueueSend(netQueue, &e, pdMS_TO_TICKS(10000)) == pdTRUE; }

static void cxSaveCursor(const String& cur) {
  cxCursor = cur;
  cxCursorText.set(cur);
  Preferences p;
  p.begin("cydbiz", false);
  p.putString("cxCur", cur);
  p.end();
}

bool connectPull(const CxCfg& c) {
  if (cxCursorReset) { cxCursorReset = false; cxSaveCursor(""); }
  String url = c.pull + (c.pull.indexOf('?') >= 0 ? "&" : "?") + "cursor=" + urlEnc(cxCursor);
  WiFiClient plain;
  WiFiClientSecure tls;
  HTTPClient http;
  if (!cxBegin(http, plain, tls, url)) { cxPullStatus.set("Pull URL must start with http:// or https://"); return false; }
  static const char* keys[] = {"X-BizBox-Signature"};
  http.collectHeaders(keys, 1);
  http.addHeader("Accept", "application/json");
  http.addHeader("X-BizBox-Device", MDNS_NAME);
  cxAddCustomHeader(http, c.header);
  int code = http.GET();
  if (code == 204) { http.end(); cxPullStatus.set("204 · nothing new" + cxClock()); return true; }
  if (code != 200) {
    http.end();
    cxPullStatus.set((code < 0 ? String("No connection") : "HTTP " + String(code)) + cxClock());
    return false;
  }
  if (http.getSize() > 16384) { http.end(); cxPullStatus.set("Reply too big (16 KB max)" + cxClock()); return false; }
  String body = http.getString();
  String sig = http.header("X-BizBox-Signature");
  http.end();

  bool signedReply = c.secret.length() && sig.length() && sameSecret(sig, "sha256=" + hmacSha256Hex(c.secret, body));
  if (c.requireSigned && !signedReply) { cxPullStatus.set("Unsigned or badly signed reply rejected" + cxClock()); return false; }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  body = "";                                         // free it before applying
  if (err) { cxPullStatus.set(String("Bad JSON: ") + err.c_str() + cxClock()); return false; }

  CxPullStats st;
  if (!connectApplyPull(doc, c, signedReply, cxPostToLoop, st)) {
    cxPullStatus.set("Screen busy, will re-read this reply" + cxClock());
    return false;
  }
  JsonVariantConst cur = jsonAt(doc.as<JsonVariantConst>(), c.pCursor);
  if (!cur.isNull()) {
    String n = cur.as<String>();
    if (n != cxCursor) cxSaveCursor(n);
  }
  String msg = "200 · " + String(st.orders) + " order" + (st.orders == 1 ? "" : "s") + ", " + String(st.commands) +
               " command" + (st.commands == 1 ? "" : "s");
  if (st.skipped) msg += ", " + String(st.skipped) + " skipped";
  cxPullStatus.set(msg + (signedReply ? " (signed)" : "") + cxClock());
  return true;
}

void connectBegin(const String& cursor) {
  cxLock = xSemaphoreCreateMutex();
  cxBootId = newApiToken().substring(0, 8);
  cxCursor = cursor;
  cxCursorText.set(cursor);
}
