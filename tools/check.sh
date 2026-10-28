#!/usr/bin/env bash
# Compile the whole sketch on a desktop against stand-in ESP32 libraries,
# for both ESP32 Arduino core 2.x and 3.x, as C++11 (core 2.x's standard),
# then run the audio-path unit test. Catches C++ and API mistakes before
# you open the Arduino IDE. It is NOT a substitute for a real ESP32 build.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
INC=(-I"$ROOT/tools/check/stubs" -I"$ROOT/tools/sim/stubs")
for core in 2 3; do
  g++ -std=gnu++11 -DFULL_CHECK -DESP_ARDUINO_VERSION_MAJOR=$core -Wall -Wextra -Wno-unused-parameter \
      "${INC[@]}" "$ROOT/tools/check/main.cpp" -o "$TMP/check$core"
  "$TMP/check$core"
  echo "core $core.x: compiles + boots in stubs"
done
g++ -std=gnu++11 -DFULL_CHECK "${INC[@]}" "$ROOT/tools/check/audio_test.cpp" -o "$TMP/audio"
"$TMP/audio"
