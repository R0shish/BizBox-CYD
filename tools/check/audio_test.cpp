// Speaker path: SAM samples -> CYDSpeaker -> DAC2 register (no timer, no ISR)
#include "Arduino.h"
#include <cassert>
#include <vector>
uint32_t g_dacReg = 0, g_dacWrites = 0; hw_timer_t g_timer; std::vector<uint8_t> g_samSamples, g_dacLog;
SerialStub Serial; EspStub ESP;
#include "ESP8266SAM.h"
#include "../../speaker.h"
static uint8_t dac() { return (g_dacReg >> 19) & 0xFF; }

int main() {
  // Parking: a slow, monotonic climb from 0 to mid-scale, once
  spk::park();
  assert(spk::parked && dac() == 128);
  for (size_t i = 1; i < g_dacLog.size(); i++) assert(g_dacLog[i] >= g_dacLog[i - 1] && g_dacLog[i] - g_dacLog[i - 1] <= 1);
  size_t parkWrites = g_dacLog.size();
  spk::park();
  assert(g_dacLog.size() == parkWrites);              // no second ramp, no thump per phrase
  printf("park: %zu steps to mid-scale\n", parkWrites);

  // Full gain passes SAM's bytes through unchanged, at the synth's pace
  CYDSpeaker out; out.SetGain(1.0f);
  g_samSamples = {128, 255, 0, 200, 60};
  g_dacLog.clear();
  uint32_t c0 = ESP.cycles;
  ESP8266SAM sam; sam.Say(&out, "x");
  std::vector<uint8_t> played(g_dacLog.begin(), g_dacLog.begin() + 5);
  printf("gain 1.0 -> "); for (int v : played) printf("%d ", v); printf("\n");
  assert(played == std::vector<uint8_t>({128, 255, 0, 200, 60}));
  uint32_t per = 240000000UL / 22050;
  printf("5 samples took %u cycles (>= %u expected)\n", ESP.cycles - c0, 4 * per);
  assert(ESP.cycles - c0 >= 4 * per);

  // stop() glides back to rest without jumps
  g_dacLog.clear();
  out.stop();
  assert(dac() == 128);
  for (size_t i = 1; i < g_dacLog.size(); i++) assert(abs((int)g_dacLog[i] - (int)g_dacLog[i - 1]) <= 1);
  printf("after stop dac=%d (rest)\n", dac());

  // Volume curve: intelligible at the default 25 %, capped at 60 % of full scale
  printf("gain: 5%%=%.2f 25%%=%.2f 50%%=%.2f 100%%=%.2f\n", voiceGainForVolume(5), voiceGainForVolume(25),
         voiceGainForVolume(50), voiceGainForVolume(100));
  assert(fabsf(voiceGainForVolume(25) - 0.30f) < 0.01f && fabsf(voiceGainForVolume(100) - 0.60f) < 0.01f);
  CYDSpeaker q; q.SetGain(voiceGainForVolume(25));
  g_samSamples = {255, 0};
  g_dacLog.clear();
  sam.Say(&q, "x");
  printf("vol 25%% -> %d %d (swing +-%d of 128)\n", g_dacLog[0], g_dacLog[1], g_dacLog[0] - 128);
  assert(g_dacLog[0] > 160 && g_dacLog[1] < 96);
  q.stop();

  // A phrase can't run away: everything past the cap returns at once
  CYDSpeaker r; r.SetGain(0.5f);
  g_samSamples.assign(spk::MAX_SECONDS * 22050 + 5000, 200);
  g_dacLog.clear();
  sam.Say(&r, "x");
  printf("cap: %u samples played of %zu given\n", r.samplesPlayed(), g_samSamples.size());
  assert(r.samplesPlayed() == spk::MAX_SECONDS * 22050);
  r.stop();
  puts("AUDIO_OK");
}
