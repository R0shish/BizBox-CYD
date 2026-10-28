#pragma once
#include "WiFi.h"
// Signatures follow arduino-esp32 2.0.17 WiFiClientSecure / 3.x NetworkClientSecure
struct WiFiClientSecure {
  void setInsecure() {} void setCACert(const char*) {} void setHandshakeTimeout(unsigned long) {}
  int connect(const char*, uint16_t) { return 0; } int connect(const char*, uint16_t, int32_t) { return 0; }
  int available() { return 0; } int read() { return -1; }
  size_t print(const String& s) { return s.length(); } size_t write(const uint8_t*, size_t n) { return n; }
  uint8_t connected() { return 0; } void stop() {}
};
