#pragma once
#include <Adafruit_GFX.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(ESP32)
#include <esp_heap_caps.h>
#endif
#include "fonts/DejaVuCondBold9pt7b.h"
#include "fonts/DejaVuBold19pt7bQ.h"
#include "fonts/DejaVuBold29pt7bNum.h"
#include "fonts/DejaVuBold49pt7bQ.h"

// A nagy felbontású puffer: ha van PSRAM, oda kerül (a belső RAM-ra a WiFi-nek és a TLS-nek van szüksége).
class SsHiCanvas : public GFXcanvas16 {
 public:
  SsHiCanvas(uint16_t w, uint16_t h) : GFXcanvas16(w, h, false) {
    const size_t bytes = (size_t)w * h * 2;
#if defined(ESP32)
    buffer = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
    if (!buffer) buffer = (uint16_t*)malloc(bytes);              // nincs PSRAM: belső RAM (ha nincs elég, az ok() hamis)
    if (buffer) memset(buffer, 0, bytes);
  }
  ~SsHiCanvas() { free(buffer); buffer = nullptr; }
};

// Élsimított vászon a kis TFT-hez: a rajzolás kétszeres (S-szeres) felbontású pufferbe megy, majd resolve()
// átlagolva kicsinyíti a valódi felbontásra. A hívó kód a megszokott, logikai (128x128) koordinátákat használja.
//  - alakzatok (kör, háromszög, téglalap): S-szeres felbontásban, a lekicsinyítés élsimítja
//  - szöveg: a setTextSize(n) a beépített 5x7-es font n-szeres méretét helyettesíti sima, arányos betűkkel
//    (1: normál szöveg, 2 és 5: nagy "?", 3: nagy számok); a kurzor y a betű TETEJE, mint a beépített fontnál
//  - a *F változatok törtszám-koordinátát fogadnak (ív, hullám, villódzó láng), a koordináta a képpont KÖZEPE
// A nagy puffer (S*S*32 kB) a PSRAM-ba kerül (a malloc 16 kB fölött oda teszi), a lekicsinyített kép 32 kB.
class SsCanvas {
 public:
  static constexpr int S = 2;

  SsCanvas(int w, int h) : w_(w), h_(h), hi_(w * S, h * S), out_((uint16_t*)calloc((size_t)w * h, 2)) {
    hi_.setTextWrap(false);
  }
  ~SsCanvas() { free(out_); }
  SsCanvas(const SsCanvas&) = delete;
  SsCanvas& operator=(const SsCanvas&) = delete;

  bool ok() const { return out_ && hi_.getBuffer(); }
  uint16_t* getBuffer() const { return out_; }          // az utolsó resolve() eredménye (w*h, RGB565)

  // ---- alakzatok (logikai koordináták, mint az Adafruit GFX-nél)
  void fillScreen(uint16_t c) { hi_.fillScreen(c); }
  void drawPixel(int x, int y, uint16_t c) { hi_.fillRect(x * S, y * S, S, S, c); }
  void drawFastHLine(int x, int y, int w, uint16_t c) { hi_.fillRect(x * S, y * S, w * S, S, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { hi_.fillRect(x * S, y * S, S, h * S, c); }
  void fillRect(int x, int y, int w, int h, uint16_t c) { hi_.fillRect(x * S, y * S, w * S, h * S, c); }
  void fillCircle(int x, int y, int r, uint16_t c) { hi_.fillCircle(x * S + S / 2, y * S + S / 2, r * S + S / 2, c); }
  void drawCircle(int x, int y, int r, uint16_t c) {            // 1 logikai képpont vastag gyűrű
    for (int k = 0; k < S; ++k) hi_.drawCircle(x * S + S / 2, y * S + S / 2, r * S + k, c);
  }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) { hi_.fillRoundRect(x * S, y * S, w * S, h * S, r * S, c); }
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    for (int k = 0; k < S; ++k) {
      const int rr = r * S - k;
      hi_.drawRoundRect(x * S + k, y * S + k, w * S - 2 * k, h * S - 2 * k, rr > 0 ? rr : 0, c);
    }
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    hi_.fillTriangle(x0 * S + S / 2, y0 * S + S / 2, x1 * S + S / 2, y1 * S + S / 2, x2 * S + S / 2, y2 * S + S / 2, c);
  }
  void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    for (int oy = 0; oy < S; ++oy) for (int ox = 0; ox < S; ++ox)
      hi_.drawTriangle(x0 * S + ox, y0 * S + oy, x1 * S + ox, y1 * S + oy, x2 * S + ox, y2 * S + oy, c);
  }

  // ---- törtszám-koordinátás változatok (a koordináta a képpont közepe)
  void fillCircleF(float x, float y, float r, uint16_t c) {
    hi_.fillCircle((int)lroundf(x * S) + S / 2, (int)lroundf(y * S) + S / 2, (int)lroundf(r * S) + S / 2, c);
  }
  void fillTriangleF(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c) {
    hi_.fillTriangle((int)lroundf(x0 * S) + S / 2, (int)lroundf(y0 * S) + S / 2, (int)lroundf(x1 * S) + S / 2,
                     (int)lroundf(y1 * S) + S / 2, (int)lroundf(x2 * S) + S / 2, (int)lroundf(y2 * S) + S / 2, c);
  }
  // Egy képpont-oszlop (a nagy felbontású vászon hx-edik oszlopa) yTop és yBot közt; a határ törtszám lehet.
  // yTop/yBot a képpont-közép koordinátarendszerben (mint a többi *F), a vég a határ, nem az utolsó képpont.
  void columnHi(int hx, float yTop, float yBot, uint16_t c) {
    const int a = (int)lroundf((yTop + 0.5f) * S), b = (int)lroundf((yBot + 0.5f) * S);
    if (b > a) hi_.fillRect(hx, a, 1, b - a, c);
  }

  // ---- szöveg
  void setTextWrap(bool) {}
  void setTextSize(uint8_t n) { size_ = n; }
  void setTextColor(uint16_t c) { col_ = c; }
  void setCursor(int x, int y) { cx_ = x; cy_ = y; }
  size_t print(const char* s) {
    hi_.setFont(fontFor(size_));
    hi_.setTextSize(1);
    hi_.setTextColor(col_);
    hi_.setCursor(cx_ * S, cy_ * S + capHi());
    return hi_.print(s);
  }
  size_t print(char c) { char b[2] = {c, 0}; return print(b); }
  void getTextBounds(const char* s, int x, int y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    hi_.setFont(fontFor(size_));
    hi_.setTextSize(1);
    int16_t hx, hy; uint16_t hw, hh;
    hi_.getTextBounds(s, 0, capHi(), &hx, &hy, &hw, &hh);
    *x1 = x + hx / S; *y1 = y + (hy - capHi()) / S;
    *w = (hw + S - 1) / S; *h = (hh + S - 1) / S;
  }

  // ---- lekicsinyítés: S*S képpont átlaga (csatornánként, RGB565-ben)
  void resolve() {
    const uint16_t* src = hi_.getBuffer();
    if (!src || !out_) return;
    const int hw = w_ * S;
    for (int y = 0; y < h_; ++y) {
      for (int x = 0; x < w_; ++x) {
        uint32_t r = 0, g = 0, b = 0;
        for (int dy = 0; dy < S; ++dy) {
          const uint16_t* p = src + (size_t)(y * S + dy) * hw + x * S;
          for (int dx = 0; dx < S; ++dx) { const uint16_t v = p[dx]; r += v >> 11; g += (v >> 5) & 63; b += v & 31; }
        }
        constexpr uint32_t n = S * S, half = n / 2;
        out_[y * w_ + x] = (uint16_t)((((r + half) / n) << 11) | (((g + half) / n) << 5) | ((b + half) / n));
      }
    }
  }

 private:
  static const GFXfont* fontFor(uint8_t sz) {
    switch (sz) {
      case 2:  return &DejaVuSans_Bold19pt7b;
      case 3:  return &DejaVuSans_Bold29pt7b;
      case 5:  return &DejaVuSans_Bold49pt7b;
      default: return &DejaVuSansCondensed_Bold9pt7b;
    }
  }
  int capHi() const { return 7 * (size_ ? size_ : 1) * S; }     // a beépített font betűmagassága (7 px) -> alapvonal

  int w_, h_;
  SsHiCanvas hi_;
  uint16_t* out_;
  uint8_t size_ = 1;
  uint16_t col_ = 0xFFFF;
  int cx_ = 0, cy_ = 0;
};
