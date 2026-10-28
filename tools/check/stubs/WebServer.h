#pragma once
#include "WiFi.h"
enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };
struct WebServer {
  WebServer(int) {}
  typedef void (*Fn)();
  void on(const char*, HTTPMethod, Fn) {} void onNotFound(Fn) {} void begin() {} void handleClient() {}
  void send(int, const char*, const String& b) { if (const char* o = getenv("OUT")) { FILE* f = fopen(o, "w"); fputs(b.c_str(), f); fclose(f); } } void send(int, const char*, const char*) {} void send(int) {}
  void sendHeader(const char*, const String&) {}
  String arg(const char*) { return ""; } bool hasArg(const char*) { return false; }
  bool authenticate(const char*, const char*) { return true; } void requestAuthentication() {}
  HTTPMethod method() { return HTTP_POST; }
};
