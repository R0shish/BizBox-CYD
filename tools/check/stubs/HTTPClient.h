#pragma once
#include "WiFiClientSecure.h"
#define HTTPC_STRICT_FOLLOW_REDIRECTS 1
struct HTTPClient { void setFollowRedirects(int) {} bool begin(WiFiClientSecure&, const String&) { return true; } int GET() { return 0; }
  String getString() { return ""; } void end() {} };
