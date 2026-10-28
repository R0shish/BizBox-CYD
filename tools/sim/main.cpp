#include "Arduino.h"
#include "TFT_eSPI.h"
#include "qrcode.h"
TFT_eSPI tft;
WiFiStub WiFi;
#include "../../model.h"
#include "../../ui.h"

static void seed() {
  cfg.bizName = "Roast & Co";
  cfg.currency = "Rs";
  cfg.storeNote = "Iced latte + croissant for Rs 380 until 4pm";
  cfg.storeOpen = true;
  cfg.qr = "000201010211153140240010002ESEWA9779801234567520454995303524580201NP5914ROAST AND CO6009KATHMANDU6304A1B2";
  cfg.qrSub = "Scan with eSewa, Khalti or any bank app";
  cfg.volume = 25; cfg.brightness = 80;
  cfg.ssid = "ShopWiFi";
  orders[0] = {101, "2x Cappuccino", 45000, "READY"};
  orders[1] = {102, "Club Sandwich", 65000, "PREP"};
  orders[2] = {103, "Iced Latte", 22000, "NEW"};
  orders[3] = {104, "Chocolate Croissant Large", 125000, "PAID"};
  orderCount = 4;
  todaySales = 48650; totalTransactions = 37; lastPayHM = "13:42";
  uint32_t h[24] = {0,0,0,0,0,0,0,1200,3400,5200,4100,3600,6900,5400,2100,0,0,0,0,0,0,0,0,0};
  for (int i = 0; i < 24; i++) hourly[i] = h[i];
  netState = NET_OK;
}

// Partial redraws must leave exactly the pixels a full redraw would
static int partialFails = 0;
static void expectSame(std::vector<uint16_t> got, const char* what) {   // a copy: the full redraw reuses tft.fb
  ui_drawScreen();
  int diff = 0;
  for (size_t i = 0; i < got.size(); i++) diff += got[i] != tft.fb[i];
  if (diff) { fprintf(stderr, "PARTIAL REDRAW MISMATCH: %s (%d px)\n", what, diff); partialFails++; }
}

static void partialTests() {
  seed();
  for (int a = 0; a < SCREEN_COUNT; a++) for (int b = 0; b < SCREEN_COUNT; b++) {
    currentScreen = (Screen)a; ui_drawScreen();
    currentScreen = (Screen)b; ui_redrawContent();
    char w[48]; snprintf(w, sizeof w, "switch %d -> %d", a, b);
    expectSame(tft.fb, w);
  }
  currentScreen = SCREEN_SETTINGS; ui_drawScreen();
  cfg.volume = 100; ui_settingValue(0, true); cfg.brightness = 10; ui_settingValue(1, true);
  expectSame(tft.fb, "volume/brightness");
  cfg.volume = 25; ui_settingValue(0, true);
  expectSame(tft.fb, "volume back");
  currentScreen = SCREEN_ORDERS; ui_drawScreen();
  for (int i = 0; i < 4; i++) {
    orders[i].status = orders[i].status == "PAID" ? "NEW" : "PAID";
    ui_orderRow(i, true); ui_ordersChip(true);
    expectSame(tft.fb, "order row tap");
  }
  currentScreen = SCREEN_STATS; ui_drawScreen();
  cfg.storeOpen = !cfg.storeOpen; ui_statsSwitch(true); expectSame(tft.fb, "store switch");
  cfg.storeOpen = !cfg.storeOpen; ui_statsSwitch(true); expectSame(tft.fb, "store switch back");
}

int main(int argc, char** argv) {
  const char* out = argc > 1 ? argv[1] : ".";
  char p[512];
  seed();
  const char* names[] = {"1_pay", "2_orders", "3_board", "4_stats", "5_setup"};
  for (int s = 0; s < SCREEN_COUNT; s++) {
    currentScreen = (Screen)s;
    ui_drawScreen();
    snprintf(p, sizeof p, "%s/%s.ppm", out, names[s]); tft.savePPM(p);
  }
  ui_paymentAlert(125000, "eSewa");
  snprintf(p, sizeof p, "%s/6_payment.ppm", out); tft.savePPM(p);
  ui_bootScreen();
  ui_bootStep(1, "Joining " + cfg.ssid, BOOT_OK);
  ui_bootStep(2, "Syncing clock", BOOT_RUN, 2);
  snprintf(p, sizeof p, "%s/7_boot.ppm", out); tft.savePPM(p);
  ui_portal();
  snprintf(p, sizeof p, "%s/8_portal.ppm", out); tft.savePPM(p);
  // Edge cases
  cfg.qr = ""; currentScreen = SCREEN_QR; ui_drawScreen();
  snprintf(p, sizeof p, "%s/9_pay_empty.ppm", out); tft.savePPM(p);
  orderCount = 0; cfg.storeOpen = false; currentScreen = SCREEN_ORDERS; ui_drawScreen();
  snprintf(p, sizeof p, "%s/10_orders_empty.ppm", out); tft.savePPM(p);
  currentScreen = SCREEN_PROMO; ui_drawScreen();
  snprintf(p, sizeof p, "%s/11_board_closed.ppm", out); tft.savePPM(p);
  ui_paymentAlert(12500000, "Fonepay transfer from Sita");
  snprintf(p, sizeof p, "%s/12_payment_big.ppm", out); tft.savePPM(p);
  ui_paymentAlert(49950, "Test", true);
  fprintf(stderr, "fonts parsed: %d\n", tft.loadCount);
  snprintf(p, sizeof p, "%s/13_payment_test.ppm", out); tft.savePPM(p);
  partialTests();
  if (partialFails) return 1;
  fprintf(stderr, "partial redraws match full redraws\n");
  return 0;
}
