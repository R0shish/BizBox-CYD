#pragma once
#include "../../sim/stubs/Arduino.h"
#include <cctype>
#include <cstdarg>
inline bool isDigit(char c) { return isdigit((unsigned char)c); }
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 1; }
inline void delayMicroseconds(uint32_t) {}
inline long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }
inline void ledcSetup(int, int, int) {}
inline void ledcAttachPin(int, int) {}
inline void ledcWrite(int, int) {}
inline void configTzTime(const char*, const char*, const char* = nullptr, const char* = nullptr) {}
struct SerialStub { void begin(int) {} void println(const String&) {} void println(const char*) {} void print(const String&) {}
  void printf(const char*, ...) {} };
extern SerialStub Serial;
struct EspStub { void restart() {} };
extern EspStub ESP;
#define IRAM_ATTR
struct hw_timer_t { void (*fn)(); uint64_t alarm; };
extern hw_timer_t g_timer;
inline hw_timer_t* timerBegin(uint32_t) { return &g_timer; }                 // core 3.x
inline hw_timer_t* timerBegin(uint8_t, uint16_t, bool) { return &g_timer; } // core 2.x
inline void timerAttachInterrupt(hw_timer_t* t, void (*f)()) { t->fn = f; }
inline void timerAttachInterrupt(hw_timer_t* t, void (*f)(), bool) { t->fn = f; }
inline void timerAlarm(hw_timer_t* t, uint64_t a, bool, uint64_t) { t->alarm = a; }
inline void timerAlarmWrite(hw_timer_t* t, uint64_t a, bool) { t->alarm = a; }
inline void timerAlarmEnable(hw_timer_t*) {}
inline void timerEnd(hw_timer_t*) {}
inline void dacWrite(uint8_t, uint8_t) {}
inline void ledcAttach(int, int, int) {}
