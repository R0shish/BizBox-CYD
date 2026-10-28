#pragma once
#include "Arduino.h"
#include <map>
#include <string>
#include <vector>
// In-memory NVS so storage code can be round-trip tested. One map for all
// namespaces (the sketch uses one). Signatures follow arduino-esp32 Preferences.
struct PrefStore { std::map<std::string, std::vector<uint8_t>> kv; };
inline PrefStore& prefStore() { static PrefStore s; return s; }
// Like the real class, reads return the default and writes do nothing unless
// begin() was called (catches reads after end()).
struct Preferences {
  bool ro = false, open = false;
  bool begin(const char*, bool readOnly = false) { ro = readOnly; open = true; return true; }
  void end() { open = false; }
  bool clear() { if (open && !ro) prefStore().kv.clear(); return open; }
  bool remove(const char* k) { return prefStore().kv.erase(k) > 0; }
  bool isKey(const char* k) { return prefStore().kv.count(k) > 0; }
  template <class T> size_t putT(const char* k, const T& v) {
    if (ro || !open) return 0;
    const uint8_t* p = (const uint8_t*)&v; prefStore().kv[k] = std::vector<uint8_t>(p, p + sizeof v); return sizeof v;
  }
  template <class T> T getT(const char* k, T d) {
    auto it = prefStore().kv.find(k);
    if (!open || it == prefStore().kv.end() || it->second.size() != sizeof(T)) return d;
    T v; memcpy(&v, it->second.data(), sizeof v); return v;
  }
  size_t putString(const char* k, const String& v) {
    if (ro || !open) return 0;
    prefStore().kv[k] = std::vector<uint8_t>(v.s.begin(), v.s.end()); return v.length();
  }
  String getString(const char* k, const String& d = String()) {
    auto it = prefStore().kv.find(k);
    return (!open || it == prefStore().kv.end()) ? d : String(std::string(it->second.begin(), it->second.end()));
  }
  size_t putBool(const char* k, bool v) { return putT(k, v); }        bool getBool(const char* k, bool d = false) { return getT(k, d); }
  size_t putInt(const char* k, int32_t v) { return putT(k, v); }      int32_t getInt(const char* k, int32_t d = 0) { return getT(k, d); }
  size_t putUInt(const char* k, uint32_t v) { return putT(k, v); }    uint32_t getUInt(const char* k, uint32_t d = 0) { return getT(k, d); }
  size_t putUChar(const char* k, uint8_t v) { return putT(k, v); }    uint8_t getUChar(const char* k, uint8_t d = 0) { return getT(k, d); }
  size_t putULong(const char* k, uint32_t v) { return putT(k, v); }   uint32_t getULong(const char* k, uint32_t d = 0) { return getT(k, d); }
  size_t putBytes(const char* k, const void* v, size_t n) {
    if (ro || !open) return 0;
    const uint8_t* p = (const uint8_t*)v; prefStore().kv[k] = std::vector<uint8_t>(p, p + n); return n;
  }
  size_t getBytesLength(const char* k) { auto it = prefStore().kv.find(k); return (!open || it == prefStore().kv.end()) ? 0 : it->second.size(); }
  size_t getBytes(const char* k, void* buf, size_t max) {
    auto it = prefStore().kv.find(k);
    if (!open || it == prefStore().kv.end() || it->second.size() > max) return 0;
    memcpy(buf, it->second.data(), it->second.size()); return it->second.size();
  }
};
