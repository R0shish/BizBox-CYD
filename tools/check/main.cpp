#include "Arduino.h"
#include <vector>
uint32_t g_dacReg = 0, g_dacWrites = 0; hw_timer_t g_timer; std::vector<uint8_t> g_samSamples;
SerialStub Serial; EspStub ESP;
#include "WiFi.h"
WiFiClass WiFi;
#include "ESPmDNS.h"
MDNSStub MDNS;
#include "../../esp.ino"
int main() { setup(); loop(); return 0; }
