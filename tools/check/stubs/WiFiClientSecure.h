#pragma once
#include "WiFi.h"
#include <functional>
#include <string>
// Signatures follow arduino-esp32 2.0.17 WiFiClientSecure / 3.x NetworkClientSecure.
// Tests can script a server: set fakeSocket() and answer each client line.
struct FakeSocket {
  bool up = false;
  std::string out;                                         // bytes waiting for the client
  std::vector<std::string> received;                       // lines the client sent
  std::function<void(FakeSocket&)> onConnect;
  std::function<void(const std::string&, FakeSocket&)> onLine;
  void send(const std::string& l) { out += l + "\r\n"; }
};
inline FakeSocket*& fakeSocket() { static FakeSocket* f = nullptr; return f; }

struct WiFiClientSecure : WiFiClient {
  std::string pending;
  void setInsecure() {} void setCACert(const char*) {} void setHandshakeTimeout(unsigned long) {}
  int connect(const char* h, uint16_t p) override { return connect(h, p, 0); }
  int connect(const char*, uint16_t, int32_t) {
    FakeSocket* f = fakeSocket();
    if (!f) return 0;
    f->up = true;
    if (f->onConnect) f->onConnect(*f);
    return 1;
  }
  int available() override { FakeSocket* f = fakeSocket(); return f ? (int)f->out.size() : 0; }
  int read() override {
    FakeSocket* f = fakeSocket();
    if (!f || f->out.empty()) return -1;
    int c = (uint8_t)f->out[0]; f->out.erase(0, 1); return c;
  }
  size_t print(const String& s) {
    FakeSocket* f = fakeSocket();
    pending += s.s;
    size_t nl;
    while ((nl = pending.find("\r\n")) != std::string::npos) {
      std::string line = pending.substr(0, nl);
      pending.erase(0, nl + 2);
      if (f) { f->received.push_back(line); if (f->onLine) f->onLine(line, *f); }
    }
    return s.length();
  }
  size_t write(const uint8_t*, size_t n) { return n; }
  uint8_t connected() override { FakeSocket* f = fakeSocket(); return f && f->up; }
  void stop() override { if (FakeSocket* f = fakeSocket()) f->up = false; }
};
