#pragma once
#include "Arduino.h"
struct Preferences {
  bool begin(const char*, bool) { return true; } void end() {} bool clear() { return true; }
  String getString(const char*, const String& d) { return d; } bool getBool(const char*, bool d) { return d; }
  int getInt(const char*, int d) { return d; } uint32_t getUInt(const char*, uint32_t d) { return d; }
  size_t getBytesLength(const char*) { return 0; } size_t getBytes(const char*, void*, size_t) { return 0; }
  size_t putString(const char*, const String&) { return 0; } size_t putBool(const char*, bool) { return 0; }
  size_t putInt(const char*, int) { return 0; } size_t putUInt(const char*, uint32_t) { return 0; }
  size_t putBytes(const char*, const void*, size_t) { return 0; }
};
