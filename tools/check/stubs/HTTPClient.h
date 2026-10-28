#pragma once
#include "WiFiClientSecure.h"
// Signatures follow arduino-esp32 2.0.17 / 3.x HTTPClient
#define HTTPC_STRICT_FOLLOW_REDIRECTS 1
struct HTTPClient {
  void setFollowRedirects(int) {} void setConnectTimeout(int32_t) {} void setTimeout(uint16_t) {}
  bool begin(WiFiClientSecure&, const String&) { return true; } int GET() { return 0; }
  String getString() { return ""; } void end() {}
};
