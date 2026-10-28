#pragma once
#include "Arduino.h"
struct JsonObject; struct JsonArray;
struct JsonVariant {
  template <class T> JsonVariant& operator=(const T&) { return *this; }
  template <class T> T as() const { return T(); }
  template <class T> bool is() const { return false; }
  bool isNull() const { return true; }
  template <class T> T to() { return T(); }
  JsonVariant operator[](const char*) const { return {}; }
  String operator|(const char* d) const { return d; }
  uint32_t operator|(int d) const { return d; }
};
struct JsonObject { JsonVariant operator[](const char*) const { return {}; } };
struct JsonArray {
  JsonObject* begin() const { return nullptr; } JsonObject* end() const { return nullptr; }
  template <class T> bool add(const T&) { return true; }
  template <class T> T add() { return T(); }
};
struct JsonDocument { JsonVariant operator[](const char*) { return {}; } };
struct DeserializationError { operator bool() const { return false; } };
inline DeserializationError deserializeJson(JsonDocument&, const String&) { return {}; }
inline size_t serializeJson(const JsonDocument&, String&) { return 0; }
