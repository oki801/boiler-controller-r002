#pragma once
#include <stdint.h>
// A TFT csonkja: a kirajzolt kép egy PC-s tömbbe kerül (preview).
#define INITR_144GREENTAB 1
extern uint16_t g_screen[128 * 128];
class Adafruit_ST7735 {
 public:
  Adafruit_ST7735(int8_t, int8_t, int8_t) {}
  void initR(uint8_t) {}
  void setRotation(uint8_t) {}
  void fillScreen(uint16_t c) { for (auto& p : g_screen) p = c; }
  void drawRGBBitmap(int16_t x, int16_t y, const uint16_t* bm, int16_t w, int16_t h) {
    for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) {
      const int X = x + i, Y = y + j;
      if (X >= 0 && X < 128 && Y >= 0 && Y < 128) g_screen[Y * 128 + X] = bm[j * w + i];
    }
  }
};
