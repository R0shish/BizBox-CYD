#pragma once
#include "Arduino.h"
#define WL_CONNECTED 3
enum { WIFI_STA = 1, WIFI_AP_STA = 3 };
struct IPAddress { String toString() const { return "192.168.1.42"; } };
struct WiFiClass {
  int status() { return WL_CONNECTED; } long RSSI() { return -58; } String SSID() { return "x"; }
  IPAddress localIP() { return {}; } IPAddress softAPIP() { return {}; }
  void mode(int) {} void begin(const char*, const char*) {} bool softAP(const char*) { return true; } void reconnect() {} uint8_t softAPgetStationNum() { return 0; }
};
extern WiFiClass WiFi;
// WiFiClient (core 3: typedef of NetworkClient)
struct WiFiClient {
  virtual ~WiFiClient() {}
  virtual int connect(const char*, uint16_t) { return 0; } virtual int available() { return 0; } virtual int read() { return -1; }
  virtual uint8_t connected() { return 0; } virtual void stop() {}
};
