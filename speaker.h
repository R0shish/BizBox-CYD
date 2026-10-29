#pragma once
// ==========================================================================
//  CYDSpeaker: plays SAM's voice out of the ESP32's DAC2 (GPIO26), the pin
//  wired to the CYD's SC8002B speaker amp.
//
//  How the board is wired (ESP32-2432S028 schematic, AUDIO block):
//    IO26 -> 4.7k -> 47k -> 0.39uF -> 22k -> amp IN-   (AC-coupled, RC low-pass)
//    The amp runs from the same 3.3 V regulator as the backlight and the
//    touch controller, NOT from the ESP32's own regulator.
//  So:
//   - The DAC's DC level doesn't matter to the amp, but every *step* in it
//     does: a jump from 0 V to mid-scale kicks the speaker hard enough to sag
//     the shared rail (backlight flicker, display/touch glitches). The DAC
//     is parked at mid-scale once, slowly, and stays there.
//   - Loud audio draws real current from that rail, so the swing is capped.
//
//  Samples are written straight from SAM's callback, paced by the CPU cycle
//  counter. There is no timer, no interrupt and no buffer to wait on, so a
//  phrase can never hang the device, and it behaves the same on core 2.x
//  and 3.x. Never touches GPIO25 (the touch clock).
//  Why not AudioOutputI2S(INTERNAL_DAC)? ESP8266Audio 2.x ignores that mode
//  and puts the I2S clock on GPIO26; AudioOutputInternalDAC uses GPIO25.
// ==========================================================================
#include <AudioOutput.h>
#include "soc/soc.h"
#include "soc/rtc_io_reg.h"

namespace spk {
const uint8_t PIN = 26;
const uint8_t MID = 128;                  // rest level (AC-coupled: any steady value is silence)
const uint32_t MAX_SECONDS = 12;          // a phrase can't hold the device longer than this
uint8_t level = 0;                        // last value sent to the DAC
bool parked = false;                      // DAC is at MID

inline void dac(uint8_t v) {
  SET_PERI_REG_BITS(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, v, RTC_IO_PDAC2_DAC_S);
  level = v;
}

// Bring the DAC to mid-scale slowly (~0.25 s) so the coupling capacitor
// charges without a thump. Done once (at boot); afterwards it stays there.
void park() {
  if (parked) return;
  dacWrite(PIN, 0);                       // powers up DAC2 and routes it to the pad
  for (int v = 0; v <= MID; v++) { dac((uint8_t)v); delayMicroseconds(2000); }
  parked = true;
}
}  // namespace spk

class CYDSpeaker : public AudioOutput {
 public:
  CYDSpeaker() { hertz = 22050; channels = 1; }

  // 0.0-1.0 of full scale (see voiceGainForVolume)
  bool SetGain(float f) override {
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    gainQ8_ = (uint16_t)(f * 256 + 0.5f);
    return true;
  }

  bool begin() override {
    spk::park();
    uint32_t hz = hertz ? hertz : 22050;
    uint32_t mhz = getCpuFrequencyMhz();
    cyclesPerSample_ = (mhz ? mhz : 240) * 1000000UL / hz;
    maxSamples_ = spk::MAX_SECONDS * hz;
    samples_ = 0;
    next_ = ESP.getCycleCount();
    return true;
  }

  // Never refuses a sample: SAM spins until this returns true.
  bool ConsumeSample(int16_t sample[2]) override {
    if (!maxSamples_) begin();
    if (samples_ >= maxSamples_) return true;      // over the cap: let SAM finish silently
    samples_++;
    int32_t v = spk::MID + ((((int32_t)sample[LEFTCHANNEL] >> 8) * (int32_t)gainQ8_) >> 8);
    uint8_t out = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    // Wait for this sample's slot; if SAM fell behind, play it now and resync
    if ((int32_t)(ESP.getCycleCount() - next_) > (int32_t)(cyclesPerSample_ * 32)) next_ = ESP.getCycleCount();
    while ((int32_t)(ESP.getCycleCount() - next_) < 0) {}
    spk::dac(out);
    next_ += cyclesPerSample_;
    return true;
  }

  // Glide back to the rest level (no click) and stay there
  bool stop() override {
    if (!spk::parked) return true;
    while (spk::level != spk::MID) {
      spk::dac(spk::level < spk::MID ? spk::level + 1 : spk::level - 1);
      delayMicroseconds(60);
    }
    return true;
  }

  uint32_t samplesPlayed() const { return samples_; }

 private:
  uint16_t gainQ8_ = 256;
  uint32_t cyclesPerSample_ = 10884;
  uint32_t next_ = 0;
  uint32_t samples_ = 0;
  uint32_t maxSamples_ = 0;
};

// Volume setting (5-100 %) to speaker swing. Speech needs a decent share of
// the DAC's 256 steps to stay intelligible (the old 25 % -> 0.1 left about
// +-12 steps, mostly noise), but full scale drives the amp to its rail and
// sags the 3.3 V rail it shares with the backlight.
// sqrt curve: 5 % -> 0.13, 25 % -> 0.30, 50 % -> 0.42, 100 % -> 0.60.
float voiceGainForVolume(int volume) {
  if (volume < 5) volume = 5;
  if (volume > 100) volume = 100;
  return 0.60f * sqrtf(volume / 100.0f);
}
