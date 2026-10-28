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

int main() {
  setup();
  token_tests();
  orders_tests();
  sales_tests();
  printf("firmware tests: %d passed, %d failed\n", passes, fails);
  if (fails) return 1;
  puts("FIRMWARE_OK");
  return 0;
}
