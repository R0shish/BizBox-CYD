// Firmware-level tests: the real sketch against the stubs (storage, orders,
// sales arithmetic). Built and run by tools/check.sh.
#include "Arduino.h"
#include <vector>
uint32_t g_dacReg = 0, g_dacWrites = 0; hw_timer_t g_timer; std::vector<uint8_t> g_samSamples;
SerialStub Serial; EspStub ESP;
#include "WiFi.h"
WiFiClass WiFi;
#include "ESPmDNS.h"
MDNSStub MDNS;
#include "../../esp.ino"

static int fails = 0, passes = 0;
#define EXPECT(cond, ...) do { if (cond) passes++; else { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void orders_tests() {
  orderCount = 0; nextOrderId = 101;
  for (int i = 0; i < MAX_ORDERS; i++) addOrder("o" + String(i), 1000, i == 3 ? "PAID" : "NEW");
  EXPECT(orderCount == MAX_ORDERS, "full");
  addOrder("new one", 500, "NEW");
  // The PAID order (o3) goes first, the unpaid o0 stays
  EXPECT(orders[0].title == "o0", "oldest unpaid kept: %s", orders[0].title.c_str());
  bool hasO3 = false;
  for (int i = 0; i < orderCount; i++) if (orders[i].title == "o3") hasO3 = true;
  EXPECT(!hasO3, "paid order dropped first");
  EXPECT(orders[orderCount - 1].title == "new one", "appended");
  addOrder("another", 0, "NEW");    // nothing paid left: oldest goes
  EXPECT(orders[0].title == "o1", "oldest dropped when none paid: %s", orders[0].title.c_str());

  orders[2].ext = "ext-42";
  orders[2].title = "Caf\xC3\xA9 latte";
  int32_t next = nextOrderId;
  saveOrdersNow();
  OrderItem snapshot[MAX_ORDERS];
  int n = orderCount;
  for (int i = 0; i < n; i++) snapshot[i] = orders[i];
  orderCount = 0; nextOrderId = 101;
  loadOrders();
  EXPECT(orderCount == n, "reload count %d vs %d", orderCount, n);
  EXPECT(nextOrderId == next, "next id %d vs %d", nextOrderId, next);
  bool same = true;
  for (int i = 0; i < n; i++)
    same &= snapshot[i].id == orders[i].id && snapshot[i].title == orders[i].title &&
            snapshot[i].amount == orders[i].amount && snapshot[i].status == orders[i].status &&
            snapshot[i].ext == orders[i].ext;
  EXPECT(same, "orders round-trip through NVS");
}

static void sales_tests() {
  todaySales = 0; salesPaisa = 0; totalTransactions = 0;
  recordPayment(50); recordPayment(50);        // 0.50 + 0.50
  EXPECT(todaySales == 1 && salesPaisa == 0, "paisa carry: %u.%02u", todaySales, salesPaisa);
  recordPayment(149975);                       // 1,499.75
  EXPECT(todaySales == 1500 && salesPaisa == 75, "1500.75: %u.%02u", todaySales, salesPaisa);
  EXPECT(totalTransactions == 3, "count");
}

static void token_tests() {
  String first = cfg.apiToken;
  EXPECT(first.length() == 32, "token made on first boot");
  loadCfg();                                   // a reboot
  EXPECT(cfg.apiToken == first, "token survives a reboot: %s vs %s", cfg.apiToken.c_str(), first.c_str());
}

// ---------- Connect ----------
static std::vector<NetEvent> posted;
static bool recordPost(const NetEvent& e) { posted.push_back(e); return true; }

static CxCfg testCx() {
  CxCfg x;
  x.hook = "https://example.com/hook"; x.secret = "s3cret"; x.header = "Authorization: Bearer abc";
  x.pull = "https://example.com/pull?shop=1"; x.everyMs = 15000; x.requireSigned = false;
  x.pOrders = "data.orders"; x.pId = "ref"; x.pTitle = "summary"; x.pAmount = "total"; x.pStatus = "state";
  x.pCommands = "cmds"; x.pCursor = "next"; x.statusMap = CX_DEFAULT_MAP;
  return x;
}

static int queuedCount() { int n = 0; for (int i = 0; i < CX_QUEUE; i++) n += cxQueue[i].used; return n; }
static CxHook* lastQueued() { CxHook* h = nullptr; for (int i = 0; i < CX_QUEUE; i++) if (cxQueue[i].used && (!h || cxQueue[i].seq > h->seq)) h = &cxQueue[i]; return h; }

static void connect_emit_tests() {
  for (int i = 0; i < CX_QUEUE; i++) cxQueue[i].used = false;
  cfg.cxHook = "https://example.com/hook"; cfg.cxEvents = 7; cfg.cxTpl = ""; cfg.currency = "Rs"; cfg.bizName = "Roast \"&\" Co";
  OrderItem* o = bookPayment("Rs 1,250.50 from SITA", 125050, "eSewa", "gmail");
  CxHook* h = lastQueued();
  EXPECT(h && h->event == "payment.received", "payment queued");
  JsonDocument d;
  EXPECT(h && !deserializeJson(d, h->body), "default body is valid JSON: %s", h ? h->body.c_str() : "");
  EXPECT(d["event"] == "payment.received" && d["shop"] == "Roast \"&\" Co" && d["test"] == false, "envelope");
  EXPECT(d["data"]["amount"].as<double>() == 1250.5 && d["data"]["amount_paisa"] == 125050, "amount %s", h->body.c_str());
  EXPECT(d["data"]["note"] == "eSewa" && d["data"]["source"] == "gmail" && d["data"]["order_id"] == o->id &&
         d["data"]["status"] == "PAID", "data fields");
  EXPECT(String(d["id"].as<const char*>()) == h->id && h->id.startsWith("evt_"), "id");

  cfg.cxTpl = "{\"text\":\"{{currency}} {{amount}} via {{note}}\",\"raw\":{{body}}}";
  bookPayment("x", 500, "Ram \"R\"", "api");
  h = lastQueued();
  JsonDocument t;
  EXPECT(!deserializeJson(t, h->body), "template output is JSON: %s", h->body.c_str());
  EXPECT(t["text"] == "Rs 5 via Ram \"R\"" && t["raw"]["event"] == "payment.received", "template fields: %s", h->body.c_str());
  cfg.cxTpl = "";

  cfg.cxEvents = CX_EVT_PAYMENT;
  int before = queuedCount();
  OrderItem* o2 = addOrder("Latte", 22000, "NEW");
  connectEmit("order.created", orderEvent(*o2, "api"));
  EXPECT(queuedCount() == before, "disabled event not queued");
  cfg.cxEvents = 7;

  for (int i = 0; i < 12; i++) connectEmit("order.created", orderEvent(*o2, "api"));
  EXPECT(queuedCount() == CX_QUEUE && cxDropped >= 4, "queue caps at %d, oldest dropped (%u)", CX_QUEUE, cxDropped);

  cfg.cxHook = "";
  EXPECT(!connectEmit("order.created", orderEvent(*o2, "api")), "no URL, no event");
  cfg.cxHook = "https://example.com/hook";
}

static String sentHeader(const char* name) {
  for (auto& kv : HTTPClient::fake().sent) if (kv.first == name) return kv.second;
  return "";
}

static void connect_delivery_tests() {
  for (int i = 0; i < CX_QUEUE; i++) cxQueue[i].used = false;
  CxCfg x = testCx();
  connectEmit("payment.received", ConnectData());
  CxHook* h = lastQueued();
  String body = h->body, id = h->id;
  uint32_t ts = h->ts;

  HTTPClient::fake().code = 503;
  EXPECT(connectDeliverDue(x), "tries a due webhook");
  EXPECT(h->used && h->tries == 1, "5xx is retried later (tries %d)", h->tries);
  EXPECT(!connectDeliverDue(x), "not due again yet");
  h->dueAt = millis();
  HTTPClient::fake().code = 200;
  EXPECT(connectDeliverDue(x), "retry delivered");
  EXPECT(!h->used, "2xx removes it");
  EXPECT(HTTPClient::fake().lastMethod == "POST" && HTTPClient::fake().lastBody == body, "body sent as queued");
  EXPECT(sentHeader("X-BizBox-Delivery") == id && sentHeader("X-BizBox-Event") == "payment.received", "id/event headers");
  EXPECT(sentHeader("Authorization") == "Bearer abc", "custom header");
  String want = "sha256=" + hmacSha256Hex("s3cret", String(ts) + "." + body);
  EXPECT(sentHeader("X-BizBox-Signature") == want, "signature header");

  connectEmit("payment.received", ConnectData());
  h = lastQueued();
  HTTPClient::fake().code = 404;
  connectDeliverDue(x);
  EXPECT(!h->used, "4xx dropped at once");
  EXPECT(cxHookStatus.get().startsWith("HTTP 404"), "status: %s", cxHookStatus.get().c_str());

  connectEmit("payment.received", ConnectData());
  h = lastQueued();
  HTTPClient::fake().code = -1;
  for (int i = 0; i < 6 && h->used; i++) { h->dueAt = millis(); connectDeliverDue(x); }
  EXPECT(!h->used, "dropped after 5 attempts");

  // RFC 4231 test case 2, through the sketch's helper
  EXPECT(hmacSha256Hex("Jefe", "what do ya want for nothing?") ==
         "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843", "HMAC-SHA256 vector");
}

static const char* PULL_JSON = R"({
  "next": 482,
  "data": {"orders": [
    {"ref": "PL-1042", "summary": "2x A5 flyers", "total": 450, "state": "Pending"},
    {"ref": 1043, "summary": "Business cards", "total": "1,200.50", "state": "printing"},
    {"ref": "PL-1044", "state": "shipped"},
    {"ref": "PL-1031", "state": "cancelled"},
    {"summary": "no id"}
  ]},
  "cmds": [
    {"id": "c-77", "type": "say", "text": "Order ten forty two is ready"},
    {"id": "c-78", "type": "screen", "screen": "orders"},
    {"id": "c-79", "type": "announce", "amount": 500, "note": "Cash"},
    {"type": "screen", "screen": "nope"},
    {"type": "reboot"}
  ]})";

static void connect_pull_tests() {
  CxCfg x = testCx();
  JsonDocument doc;
  EXPECT(!deserializeJson(doc, PULL_JSON), "pull fixture parses");
  posted.clear();
  CxPullStats st;
  EXPECT(connectApplyPull(doc, x, false, recordPost, st), "apply unsigned");
  EXPECT(st.orders == 4 && st.commands == 2, "orders %d commands %d", st.orders, st.commands);
  EXPECT(posted.size() == 6, "events %d", (int)posted.size());
  const NetEvent& a = posted[0];
  EXPECT(a.type == EV_ORDER && String(a.ext) == "PL-1042" && String(a.text) == "2x A5 flyers" && a.paisa == 45000 &&
         a.status == 0 && a.has == (HAS_TITLE | HAS_AMOUNT | HAS_STATUS), "order 1 mapped");
  EXPECT(String(posted[1].ext) == "1043" && posted[1].paisa == 120050 && posted[1].status == 1, "numeric id, string amount, printing->PREP");
  EXPECT(posted[2].has == 0, "unknown status and no other fields: has=%d", posted[2].has);
  EXPECT(posted[3].has == HAS_STATUS && posted[3].status == 4, "cancelled -> REMOVE");
  EXPECT(posted[4].type == EV_SAY && String(posted[4].text) == "Order ten forty two is ready", "say");
  EXPECT(posted[5].type == EV_SCREEN && posted[5].status == 1, "screen orders");
  EXPECT(st.skipped == 4, "skipped: no id, unsigned announce, bad screen, unknown type (%d)", st.skipped);

  // Same reply again: commands with an id are not run twice; orders are idempotent
  posted.clear(); CxPullStats st2;
  connectApplyPull(doc, x, true, recordPost, st2);
  int says = 0, announces = 0;
  for (auto& e : posted) { says += e.type == EV_SAY; announces += e.type == EV_ANNOUNCE; }
  EXPECT(says == 0, "say not repeated");
  EXPECT(announces == 1, "signed announce runs");
  for (auto& e : posted) if (e.type == EV_ANNOUNCE) EXPECT(e.paisa == 50000 && String(e.note) == "Cash", "announce fields");

  // Into the order list
  orderCount = 0;
  todaySales = 0; totalTransactions = 0;
  posted.clear(); { CxPullStats s3; connectApplyPull(doc, x, false, recordPost, s3); }
  for (auto& e : posted) if (e.type == EV_ORDER) applyExternalOrder(e);
  EXPECT(orderCount == 3, "3 orders shown (cancelled one never added): %d", orderCount);
  EXPECT(orders[0].ext == "PL-1042" && orders[0].status == "NEW" && orders[0].amount == 45000, "first order");
  EXPECT(orders[2].ext == "PL-1044" && orders[2].title == "Order PL-1044" && orders[2].status == "NEW", "fallback title/status");
  NetEvent upd; memset(&upd, 0, sizeof upd);
  upd.type = EV_ORDER; strcpy(upd.ext, "PL-1042"); upd.has = HAS_STATUS; upd.status = 3;
  applyExternalOrder(upd);
  EXPECT(orders[0].status == "PAID" && orders[0].title == "2x A5 flyers", "status-only update keeps the title");
  EXPECT(todaySales == 0 && totalTransactions == 0, "external PAID doesn't touch today's totals");
  upd.status = 4; applyExternalOrder(upd);
  EXPECT(orderCount == 2 && orders[0].ext == "1043", "REMOVE takes it off");

  // Whole pull through the fake HTTP client: cursor, signature
  HTTPClient::fake().code = 200;
  HTTPClient::fake().body = "{\"next\":\"c-9\",\"data\":{\"orders\":[]},\"cmds\":[]}";
  HTTPClient::fake().sig = "";
  cxCursor = "c-8";
  EXPECT(connectPull(x), "pull ok");
  EXPECT(HTTPClient::fake().lastUrl == "https://example.com/pull?shop=1&cursor=c-8", "url %s", HTTPClient::fake().lastUrl.c_str());
  EXPECT(cxCursor == "c-9", "cursor advanced");
  Preferences p; p.begin("cydbiz", true);
  EXPECT(p.getString("cxCur", "") == "c-9", "cursor persisted");
  p.end();
  x.requireSigned = true;
  EXPECT(!connectPull(x), "unsigned reply rejected when required");
  HTTPClient::fake().sig = "sha256=" + hmacSha256Hex("s3cret", HTTPClient::fake().body);
  EXPECT(connectPull(x), "signed reply accepted: %s", cxPullStatus.get().c_str());
  HTTPClient::fake().body = "{not json";
  EXPECT(!connectPull(testCx()), "bad JSON reported");
  EXPECT(cxPullStatus.get().startsWith("Bad JSON"), "status %s", cxPullStatus.get().c_str());
  HTTPClient::fake().code = 204;
  EXPECT(connectPull(testCx()), "204 is fine");
}

int main() {
  setup();
  connect_emit_tests();
  connect_delivery_tests();
  connect_pull_tests();
  token_tests();
  orders_tests();
  sales_tests();
  printf("firmware tests: %d passed, %d failed\n", passes, fails);
  if (fails) return 1;
  puts("FIRMWARE_OK");
  return 0;
}
