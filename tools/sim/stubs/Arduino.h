// Minimal Arduino stand-ins so the real UI code compiles on a desktop.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <string>
#include <algorithm>
#include <cctype>

using std::min;
using std::max;
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
template <class T, class L, class H> T constrain(T v, L lo, H hi) { return v < lo ? lo : v > hi ? hi : v; }

class String {
 public:
  std::string s;
  String() {}
  String(const char* c) : s(c ? c : "") {}
  String(const char* c, unsigned n) : s(c, n) {}
  String(const std::string& x) : s(x) {}
  // Explicit like the real WString.h, so code that only compiles here fails here too
  explicit String(char c) : s(1, c) {}
  explicit String(int v) : s(std::to_string(v)) {}
  explicit String(unsigned v) : s(std::to_string(v)) {}
  explicit String(long v) : s(std::to_string(v)) {}
  explicit String(unsigned long v) : s(std::to_string(v)) {}
  explicit String(long long v) : s(std::to_string(v)) {}
  explicit String(unsigned long long v) : s(std::to_string(v)) {}
  unsigned length() const { return s.size(); }
  bool reserve(unsigned n) { s.reserve(n); return true; }
  const char* c_str() const { return s.c_str(); }
  bool isEmpty() const { return s.empty(); }
  String substring(unsigned a) const { return a >= s.size() ? String() : String(s.substr(a)); }
  String substring(unsigned a, unsigned b) const {
    if (a > b) std::swap(a, b);
    if (a >= s.size()) return String();
    return String(s.substr(a, std::min<size_t>(b, s.size()) - a));
  }
  int indexOf(char c, unsigned from = 0) const { auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const String& c, unsigned from = 0) const { auto p = s.find(c.s, from); return p == std::string::npos ? -1 : (int)p; }
  void remove(unsigned i) { if (i < s.size()) s.erase(i); }
  void remove(unsigned i, unsigned n) { if (i < s.size()) s.erase(i, n); }
  void trim() {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) { s.clear(); return; }
    size_t b = s.find_last_not_of(" \t\r\n");
    s = s.substr(a, b - a + 1);
  }
  int lastIndexOf(char c) const { auto p = s.rfind(c); return p == std::string::npos ? -1 : (int)p; }
  char charAt(unsigned i) const { return i < s.size() ? s[i] : 0; }
  void setCharAt(unsigned i, char c) { if (i < s.size()) s[i] = c; }
  bool concat(const char* c, unsigned n) { s.append(c, n); return true; }
  bool concat(const char* c) { if (c) s.append(c); return true; }
  String& operator=(const char* c) { s = c ? c : ""; return *this; }
  bool equalsIgnoreCase(const String& o) const {
    if (o.s.size() != s.size()) return false;
    for (size_t i = 0; i < s.size(); i++) if (tolower((unsigned char)s[i]) != tolower((unsigned char)o.s[i])) return false;
    return true;
  }
  void toUpperCase() { for (auto& c : s) c = toupper((unsigned char)c); }
  void toLowerCase() { for (auto& c : s) c = tolower((unsigned char)c); }
  void replace(const String& a, const String& b) {
    if (a.s.empty()) return;
    size_t p = 0;
    while ((p = s.find(a.s, p)) != std::string::npos) { s.replace(p, a.s.size(), b.s); p += b.s.size(); }
  }
  bool endsWith(const String& x) const { return s.size() >= x.s.size() && s.compare(s.size() - x.s.size(), x.s.size(), x.s) == 0; }
  bool startsWith(const String& x) const { return s.rfind(x.s, 0) == 0; }
  long toInt() const { return atol(s.c_str()); }
  char operator[](unsigned i) const { return s[i]; }
  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(const char* o) { s += o; return *this; }
  String& operator+=(char c) { s += c; return *this; }
  String& operator+=(int v) { s += std::to_string(v); return *this; }
  String& operator+=(unsigned v) { s += std::to_string(v); return *this; }
  String& operator+=(long v) { s += std::to_string(v); return *this; }
  String& operator+=(unsigned long v) { s += std::to_string(v); return *this; }
  bool operator==(const String& o) const { return s == o.s; }
  bool operator==(const char* o) const { return s == o; }
  bool operator!=(const String& o) const { return s != o.s; }
  bool operator!=(const char* o) const { return s != o; }
};
inline String operator+(const String& a, const String& b) { return String(a.s + b.s); }
inline String operator+(const String& a, const char* b) { return String(a.s + b); }
inline String operator+(const char* a, const String& b) { return String(std::string(a) + b.s); }
inline String operator+(const String& a, char c) { return String(a.s + c); }

inline uint32_t millis() { return 0; }
inline void delay(uint32_t) {}

// Time: fixed to 13:49:02 so previews are stable
inline bool getLocalTime(struct tm* t, uint32_t = 0) {
  memset(t, 0, sizeof(*t));
  t->tm_hour = 13; t->tm_min = 49; t->tm_sec = 2; t->tm_year = 126; t->tm_yday = 300;
  return true;
}

#ifndef FULL_CHECK
// WiFi
#define WL_CONNECTED 3
struct IPAddressStub { String toString() const { return "192.168.1.42"; } };
struct WiFiStub {
  int st = WL_CONNECTED;
  int status() { return st; }
  long RSSI() { return -58; }
  String SSID() { return "ShopWiFi"; }
  IPAddressStub localIP() { return {}; }
};
extern WiFiStub WiFi;
#endif
