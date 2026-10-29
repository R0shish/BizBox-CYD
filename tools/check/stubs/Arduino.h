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
// Cycle counter advances on every read (1 us at 240 MHz) so pacing loops end
struct EspStub { void restart() {} uint32_t cycles = 0; uint32_t getCycleCount() { return cycles += 240; } };
inline uint32_t getCpuFrequencyMhz() { return 240; }
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

// FreeRTOS (pulled in by Arduino.h on the ESP32). The task never runs here.
typedef void* QueueHandle_t; typedef void* SemaphoreHandle_t; typedef void* TaskHandle_t;
typedef uint32_t TickType_t; typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
struct portMUX_TYPE { int x; };
#define portMUX_INITIALIZER_UNLOCKED {0}
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
inline QueueHandle_t xQueueCreate(uint32_t, uint32_t) { return (QueueHandle_t)1; }
inline BaseType_t xQueueSend(QueueHandle_t, const void*, TickType_t) { return pdTRUE; }
inline BaseType_t xQueueReceive(QueueHandle_t, void*, TickType_t) { return pdFALSE; }
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (SemaphoreHandle_t)1; }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return pdTRUE; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
inline BaseType_t xTaskCreatePinnedToCore(void (*)(void*), const char*, uint32_t, void*, uint32_t, TaskHandle_t*, BaseType_t) { return pdPASS; }
inline void vTaskDelay(TickType_t) {}
