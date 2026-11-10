#!/usr/bin/env bash
# PC-s kijelző-előnézet. Használat:  tools/display_preview/build.sh [kimeneti-mappa]
# Szükséges: g++, gcc, python3 + Pillow, internet az első futáskor (Adafruit GFX, ArduinoJson és U8g2_for_Adafruit_GFX, a /tmp-be kerülnek).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
OUT="${1:-/tmp/preview}"; mkdir -p "$OUT"
[ -d /tmp/gfx ]         || git clone --depth 1 -q https://github.com/adafruit/Adafruit-GFX-Library.git /tmp/gfx
[ -d /tmp/ArduinoJson ] || git clone --depth 1 -q https://github.com/bblanchon/ArduinoJson.git /tmp/ArduinoJson
[ -d /tmp/u8g2fa ]      || git clone --depth 1 -q https://github.com/olikraus/U8g2_for_Adafruit_GFX.git /tmp/u8g2fa
# a U8g2 betűkészletek C fájlja külön (gcc), a többi g++
gcc -O1 -w -c /tmp/u8g2fa/src/u8g2_fonts.c -I/tmp/u8g2fa/src -o "$OUT/u8g2_fonts.o"
g++ -std=gnu++17 -O1 -w -fno-access-control -DNATIVE_BUILD -DARDUINO=10800 -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 \
  -I"$HERE/stubs" -I/tmp/gfx/src -I/tmp/ArduinoJson/src -I/tmp/u8g2fa/src \
  -I"$ROOT/include" -I"$ROOT/lib/Core" -I"$ROOT/lib/Hal" -I"$ROOT/lib/Display" -I"$ROOT/lib/Display/fonts" \
  "$HERE/preview.cpp" "$ROOT/lib/Display/Display.cpp" /tmp/gfx/src/Adafruit_GFX.cpp /tmp/u8g2fa/src/U8g2_for_Adafruit_GFX.cpp "$OUT/u8g2_fonts.o" \
  -o "$OUT/preview.bin"
"$OUT/preview.bin" "$OUT"
python3 "$HERE/render_png.py" "$OUT"
