#include "Arduino.h"
#include <vector>
uint32_t g_dacReg = 0, g_dacWrites = 0; hw_timer_t g_timer; std::vector<uint8_t> g_samSamples, g_dacLog;
SerialStub Serial; EspStub ESP;
#include "WiFi.h"
WiFiClass WiFi;
#include "ESPmDNS.h"
MDNSStub MDNS;
#include "../../esp.ino"
int main(int argc, char** argv) {
  cfg.bizName = "Roast & Co"; cfg.currency = "Rs"; cfg.storeOpen = true; cfg.volume = 25; cfg.brightness = 80;
  cfg.qrSub = "Scan with eSewa, Khalti or any bank app"; cfg.storeNote = "Iced latte + croissant for Rs 380 until 4pm";
  cfg.gmailUser = "you@example.com"; cfg.imapQuery = "UNSEEN FROM \"esewa\""; cfg.ssid = "ShopWiFi";
  cfg.verifyGoogle = true; cfg.apiToken = "3f9c0d1e7a2b4c5d6e7f8091a2b3c4d5"; formNonce = "nonce";
  cfg.cxHook = "https://example.com/bizbox"; cfg.cxEvents = 7; cfg.cxEvery = 15; cfg.cxSecret = "s3cret";
  cfg.cxPOrders = "orders"; cfg.cxPId = "id"; cfg.cxPTitle = "title"; cfg.cxPAmount = "amount"; cfg.cxPStatus = "status";
  cfg.cxPCommands = "commands"; cfg.cxPCursor = "cursor"; cfg.cxMap = CX_DEFAULT_MAP;
  cxHookStatus.set("HTTP 200 · payment.received 13:42:05");
  netSetStatus("Connected - inbox clean"); netState = NET_OK;
  orders[0] = {101, "2x Cappuccino", 45000, "READY"}; orders[1] = {102, "Club Sandwich", 65000, "PREP"};
  orders[2] = {103, "Iced Latte", 22000, "NEW"}; orders[3] = {104, "Chocolate Croissant", 125000, "PAID"}; orderCount = 4;
  todaySales = 48650; totalTransactions = 37; lastPayHM = "13:42";
  uint32_t h[24] = {0,0,0,0,0,0,0,1200,3400,5200,4100,3600,6900,5400,2100,0,0,0,0,0,0,0,0,0};
  for (int i = 0; i < 24; i++) hourly[i] = h[i];
  if (argc > 1) portalMode = true;
  handleRoot();
}
