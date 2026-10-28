#pragma once
#include "WiFi.h"
#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)
typedef const char* PGM_P;
enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };
struct WebServer {
  WebServer(int) {}
  typedef void (*Fn)();
  void on(const char*, HTTPMethod, Fn) {} void onNotFound(Fn) {} void begin() {} void handleClient() {}
  // Responses go to the file named by $OUT so tools can render the dashboard
  static void out(const char* mode, const char* b) { if (const char* o = getenv("OUT")) { FILE* f = fopen(o, mode); fputs(b, f); fclose(f); } }
  void send(int, const char*, const String& b) { out("w", b.c_str()); } void send(int, const char*, const char* b) { out("w", b); } void send(int) {}
  void setContentLength(size_t) {}
  void sendContent(const String& b) { out("a", b.c_str()); } void sendContent_P(const char* b) { out("a", b); }
  void sendHeader(const char*, const String&) {}
  String arg(const char*) { return ""; } bool hasArg(const char*) { return false; }
  String header(const char*) { return ""; } bool hasHeader(const char*) { return false; }
  bool authenticate(const char*, const char*) { return true; } void requestAuthentication() {}
  HTTPMethod method() { return HTTP_POST; }
};
