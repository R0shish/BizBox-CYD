#pragma once
#include "AudioOutput.h"
#include <vector>
extern std::vector<uint8_t> g_samSamples;   // test hook: bytes SAM "renders"
struct ESP8266SAM {
  void SetSpeed(uint8_t) {} void SetPitch(uint8_t) {} void SetMouth(uint8_t) {} void SetThroat(uint8_t) {}
  bool Say(AudioOutput* out, const char*) {
  out->SetRate(22050); out->SetChannels(1); out->begin();
  for (uint8_t b : g_samSamples) { int16_t s[2]; s[0] = (((int16_t)(b & 0xff)) - 128) << 8; s[1] = s[0]; while (!out->ConsumeSample(s)) {} }
  return true; } };
