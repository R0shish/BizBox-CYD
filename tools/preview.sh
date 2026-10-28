#!/usr/bin/env bash
# Render every device screen (and, with --web, the phone dashboard) to PNGs.
# Needs: g++, python3 + Pillow (and Playwright/Chromium for --web;
# set CHROMIUM=/path/to/chrome to use an already installed browser).
#   tools/preview.sh            -> previews/*.png
#   tools/preview.sh --web      -> also dashboard screenshots
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/previews"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++11 -O1 -Wall -Wno-unused-function -I"$ROOT/tools/sim/stubs" "$ROOT/tools/sim/main.cpp" -o "$TMP/sim"
"$TMP/sim" "$TMP"
python3 "$ROOT/tools/sim/to_png.py" "$TMP" "$OUT"

if [[ "${1:-}" == "--web" ]]; then
  gcc -c "$ROOT/qr_encode.c" -o "$TMP/qr_encode.o"
  AJ="${ARDUINOJSON_SRC:-$HOME/Arduino/libraries/ArduinoJson/src}"   # real ArduinoJson 7, see tools/check.sh
  g++ -std=gnu++11 -DFULL_CHECK -I"$ROOT/tools/check/stubs" -I"$ROOT/tools/sim/stubs" -I"$AJ" -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 \
      "$ROOT/tools/check/web_main.cpp" "$TMP/qr_encode.o" -o "$TMP/web"
  OUT="$TMP/dash.html" "$TMP/web"
  OUT="$TMP/portal.html" "$TMP/web" portal
  python3 "$ROOT/tools/check/shot.py" "$TMP" "$OUT"
  echo "dashboard screenshots -> $OUT"
fi
