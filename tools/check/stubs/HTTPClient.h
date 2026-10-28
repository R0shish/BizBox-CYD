#pragma once
#include <vector>
#include "WiFiClientSecure.h"
// Signatures follow arduino-esp32 2.0.17 / 3.x HTTPClient. Requests "fail"
// with a connection error (-1) unless a test sets HTTPClient::fake.
#define HTTPC_STRICT_FOLLOW_REDIRECTS 1
struct HTTPFake { int code = -1; String body; String sig; String lastUrl, lastBody, lastMethod; int calls = 0;
  std::vector<std::pair<String, String>> sent; };
struct HTTPClient {
  static HTTPFake& fake() { static HTTPFake f; return f; }
  void setFollowRedirects(int) {} void setConnectTimeout(int32_t) {} void setTimeout(uint16_t) {} void setReuse(bool) {}
  void setUserAgent(const String&) {}
  bool begin(WiFiClient&, const String& url) { fake().lastUrl = url; fake().sent.clear(); return true; }
  void addHeader(const String& n, const String& v, bool = false, bool = true) { fake().sent.push_back(std::make_pair(n, v)); }
  void collectHeaders(const char* [], const size_t) {}
  String header(const char* n) { return String(n) == "X-BizBox-Signature" ? fake().sig : String(""); }
  int GET() { fake().calls++; fake().lastMethod = "GET"; return fake().code; }
  int POST(String b) { fake().calls++; fake().lastMethod = "POST"; fake().lastBody = b; return fake().code; }
  int getSize() { return (int)fake().body.length(); }
  String getString() { return fake().body; } void end() {}
};
