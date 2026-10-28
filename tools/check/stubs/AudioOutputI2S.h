#pragma once
#include "AudioOutput.h"
struct AudioOutputI2S : AudioOutput { enum { EXTERNAL_I2S = 0, INTERNAL_DAC = 1 }; AudioOutputI2S(int, int) {}
  bool SetOutputModeMono(bool) { return true; } bool stop() override { return true; } };
