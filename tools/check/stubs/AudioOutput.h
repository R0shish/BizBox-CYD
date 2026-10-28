#pragma once
#include "Arduino.h"
class AudioOutput {
 public:
  virtual ~AudioOutput() {}
  virtual bool SetRate(int hz) { hertz = hz; return true; }
  virtual bool SetChannels(int c) { channels = c; return true; }
  virtual bool SetGain(float f) { if (f > 4) f = 4; if (f < 0) f = 0; gainF2P6 = (uint8_t)(f * 64); return true; }
  virtual bool begin() { return false; }
  typedef enum { LEFTCHANNEL = 0, RIGHTCHANNEL = 1 } SampleIndex;
  virtual bool ConsumeSample(int16_t s[2]) { return false; }
  virtual bool stop() { return false; }
 protected:
  uint16_t hertz; uint8_t channels; uint8_t gainF2P6;
};
