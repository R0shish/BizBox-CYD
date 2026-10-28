#pragma once
// ==========================================================================
//  CYDSpeaker: plays SAM's voice straight out of the ESP32's DAC2 (GPIO26),
//  the pin wired to the CYD's speaker amp.
//
//  Why not AudioOutputI2S(INTERNAL_DAC)? In ESP8266Audio 2.x that mode is
//  ignored and the class sets up normal external I2S with BCLK on GPIO26,
//  so the speaker gets the I2S clock mixed with the voice (heavy distortion).
//  Its replacement, AudioOutputInternalDAC, outputs mono on GPIO25 instead
//  (the touch clock pin on the CYD). This class sidesteps both: a hardware
//  timer clocks 8-bit samples into DAC2 at the synth's rate from a ring
//  buffer. It never touches GPIO25, so touch keeps working during speech.
//  Works with ESP32 Arduino core 2.x and 3.x and any ESP8266Audio version.
// ==========================================================================
#include <AudioOutput.h>
#include "soc/soc.h"
#include "soc/rtc_io_reg.h"

namespace spk {
const uint16_t SIZE = 2048;          // ~93 ms at 22.05 kHz, absorbs SAM's bursts
volatile uint8_t ring[SIZE];
volatile uint16_t head = 0;          // written by ConsumeSample
volatile uint16_t tail = 0;          // read by the timer ISR
volatile uint8_t level = 0;          // last value sent to the DAC
hw_timer_t* timer = nullptr;

void IRAM_ATTR onTick() {
  if (tail != head) {
    level = ring[tail];
    tail = (tail + 1) & (SIZE - 1);
    SET_PERI_REG_BITS(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, level, RTC_IO_PDAC2_DAC_S);
  }
}

// Slide the DAC between levels so the amp doesn't pop
void ramp(uint8_t from, uint8_t to) {
  int step = to > from ? 1 : -1;
  for (int v = from; v != to; v += step) {
    SET_PERI_REG_BITS(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, (uint8_t)v, RTC_IO_PDAC2_DAC_S);
    delayMicroseconds(80);
  }
  SET_PERI_REG_BITS(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, to, RTC_IO_PDAC2_DAC_S);
  level = to;
}
}  // namespace spk

class CYDSpeaker : public AudioOutput {
 public:
  static const uint8_t PIN = 26;

  CYDSpeaker() { hertz = 22050; channels = 1; SetGain(1.0f); }
  ~CYDSpeaker() { stop(); }

  bool begin() override {
    if (spk::timer) return true;
    spk::head = spk::tail = 0;
    dacWrite(PIN, 0);                 // powers up DAC2 and routes it to the pad
    spk::ramp(0, 128);                // rest at mid-scale = silence
    uint32_t period = 40000000UL / (hertz ? hertz : 22050);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    spk::timer = timerBegin(40000000);
    timerAttachInterrupt(spk::timer, &spk::onTick);
    timerAlarm(spk::timer, period, true, 0);
#else
    spk::timer = timerBegin(0, 2, true);          // 80 MHz / 2 = 40 MHz
    timerAttachInterrupt(spk::timer, &spk::onTick, true);
    timerAlarmWrite(spk::timer, period, true);
    timerAlarmEnable(spk::timer);
#endif
    return true;
  }

  bool ConsumeSample(int16_t sample[2]) override {
    if (!spk::timer) begin();
    uint16_t next = (spk::head + 1) & (spk::SIZE - 1);
    if (next == spk::tail) return false;          // full: SAM yields and retries
    int32_t v = 128 + ((((int32_t)sample[LEFTCHANNEL] >> 8) * gainF2P6) >> 6);
    spk::ring[spk::head] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    spk::head = next;
    return true;
  }

  bool stop() override {
    if (!spk::timer) return true;
    uint32_t t0 = millis();
    while (spk::tail != spk::head && millis() - t0 < 500) delay(1);  // let it finish
    timerEnd(spk::timer);
    spk::timer = nullptr;
    spk::ramp(spk::level, 0);         // back to 0 V so the amp sits quiet
    return true;
  }
};
