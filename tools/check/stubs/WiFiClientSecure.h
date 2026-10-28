#pragma once
#include "WiFi.h"
struct WiFiClientSecure { void setInsecure() {} void setTimeout(int) {} bool connect(const char*, int) { return false; }
  String readStringUntil(char) { return ""; } void print(const String&) {} bool connected() { return false; } void stop() {} };
