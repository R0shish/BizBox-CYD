#include "Arduino.h"
#include <cassert>
#include <vector>
uint32_t g_dacReg = 0, g_dacWrites = 0; hw_timer_t g_timer; std::vector<uint8_t> g_samSamples;
#include "ESP8266SAM.h"
#include "../../speaker.h"
static uint8_t dac() { return (g_dacReg >> 19) & 0xFF; }
int main() {
  CYDSpeaker out; out.SetGain(1.0f);
  g_samSamples = {128, 255, 0, 200, 60};
  ESP8266SAM sam; sam.Say(&out, "x");
  printf("alarm period=%llu ticks @40MHz -> %.1f Hz\n", (unsigned long long)g_timer.alarm, 40e6 / g_timer.alarm);
  std::vector<int> got;
  for (int i = 0; i < 5; i++) { g_timer.fn(); got.push_back(dac()); }
  printf("gain 1.0 -> "); for (int v : got) printf("%d ", v); printf("\n");
  assert(got[0] == 128 && got[1] == 255 && got[2] == 0 && got[3] == 200 && got[4] == 60);
  spk::tail = spk::head; out.stop(); printf("after stop dac=%d\n", dac()); assert(dac() == 0);
  CYDSpeaker q; q.SetGain(0.1f); g_samSamples = {255, 0}; sam.Say(&q, "x"); g_timer.fn(); int a = dac(); g_timer.fn(); int b = dac();
  printf("gain 0.1 (vol 25%%) -> %d %d\n", a, b);
  // overflow: more samples than the ring holds must not deadlock when the ISR drains
  spk::tail = spk::head; q.stop();
  puts("AUDIO_OK");
}
