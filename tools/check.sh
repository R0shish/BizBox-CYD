#!/usr/bin/env bash
# Compile the whole sketch on a desktop against stand-in ESP32 libraries,
# for both ESP32 Arduino core 2.x and 3.x, as C++11 (core 2.x's standard),
# then run the audio, text/amount, firmware and Connect unit tests. Catches
# C++ and API mistakes before you open the Arduino IDE. It is NOT a
# substitute for a real ESP32 build (CI does that, see .github/workflows).
#
# Needs the real ArduinoJson 7 (header-only). Found in $ARDUINOJSON_SRC or
# your Arduino libraries folder; or: git clone -b v7.4.2 --depth 1
# https://github.com/bblanchon/ArduinoJson and export ARDUINOJSON_SRC=.../src
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

AJ=""
for d in "${ARDUINOJSON_SRC:-}" "$HOME/Arduino/libraries/ArduinoJson/src" "$HOME/Documents/Arduino/libraries/ArduinoJson/src"; do
  if [[ -n "$d" && -f "$d/ArduinoJson.h" ]]; then AJ="$d"; break; fi
done
if [[ -z "$AJ" ]]; then
  echo "ArduinoJson 7 not found: set ARDUINOJSON_SRC to its src/ folder (see the top of this script)" >&2
  exit 2
fi

INC=(-I"$ROOT/tools/check/stubs" -I"$ROOT/tools/sim/stubs" -I"$AJ" -DARDUINOJSON_ENABLE_ARDUINO_STRING=1)
gcc -c -O1 "$ROOT/qr_encode.c" -o "$TMP/qr_encode.o"
for core in 2 3; do
  g++ -std=gnu++11 -DFULL_CHECK -DESP_ARDUINO_VERSION_MAJOR=$core -Wall -Wextra -Wno-unused-parameter \
      "${INC[@]}" "$ROOT/tools/check/main.cpp" "$TMP/qr_encode.o" -o "$TMP/check$core"
  "$TMP/check$core"
  echo "core $core.x: compiles + boots in stubs"
done
g++ -std=gnu++11 -DFULL_CHECK "${INC[@]}" "$ROOT/tools/check/audio_test.cpp" -o "$TMP/audio"
"$TMP/audio"
g++ -std=gnu++11 -Wall -Wextra -Wno-unused-parameter -I"$ROOT/tools/sim/stubs" "$ROOT/tools/check/logic_test.cpp" -o "$TMP/logic"
"$TMP/logic"
g++ -std=gnu++11 -DFULL_CHECK -DESP_ARDUINO_VERSION_MAJOR=3 "${INC[@]}" "$ROOT/tools/check/firmware_test.cpp" "$TMP/qr_encode.o" -o "$TMP/firmware"
"$TMP/firmware"
