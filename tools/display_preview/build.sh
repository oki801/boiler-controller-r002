#!/usr/bin/env bash
# PC-s kijelző-előnézet. Használat:  tools/display_preview/build.sh [kimeneti-mappa]
# Szükséges: g++, python3 + Pillow, internet az első futáskor (Adafruit GFX és ArduinoJson, a /tmp-be kerülnek).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
OUT="${1:-/tmp/preview}"; mkdir -p "$OUT"
[ -d /tmp/gfx ]         || git clone --depth 1 -q https://github.com/adafruit/Adafruit-GFX-Library.git /tmp/gfx
[ -d /tmp/ArduinoJson ] || git clone --depth 1 -q https://github.com/bblanchon/ArduinoJson.git /tmp/ArduinoJson
g++ -std=gnu++17 -O1 -w -fno-access-control -DNATIVE_BUILD -DARDUINO=10800 -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 \
  -I"$HERE/stubs" -I/tmp/gfx/src -I/tmp/ArduinoJson/src \
  -I"$ROOT/include" -I"$ROOT/lib/Core" -I"$ROOT/lib/Hal" -I"$ROOT/lib/Display" -I"$ROOT/lib/Display/fonts" \
  "$HERE/preview.cpp" "$ROOT/lib/Display/Display.cpp" /tmp/gfx/src/Adafruit_GFX.cpp \
  -o "$OUT/preview.bin"
"$OUT/preview.bin" "$OUT"
python3 "$HERE/render_png.py" "$OUT"
