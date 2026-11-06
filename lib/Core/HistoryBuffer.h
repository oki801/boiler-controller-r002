#pragma once
// Rövid távú előzmény: körpuffer rögzített időközű mintákkal (hardver nélkül tesztelhető).
// A puffert a hívó adja (ESP32-n a PSRAM-ból), így a kapacitás a memóriától függ.
#include <stdint.h>
#include <stddef.h>
#include <limits.h>

constexpr int16_t kHistNoTemp = INT16_MIN;

struct HistSample {
  int16_t  tempX10;     // 0,1 °C egység; kHistNoTemp = nincs érvényes mérés
  uint16_t pW;          // teljesítmény, W
  uint16_t vX10;        // feszültség, 0,1 V egység
  uint8_t  active;      // 0 = egyik sem, 1..3 = aktív hálózat
  uint8_t  flags;       // bit0: van aktív hiba
};

class HistoryBuffer {
 public:
  struct Bucket {
    int16_t  tempX10 = kHistNoTemp;
    uint16_t pW = 0, vX10 = 0;
    uint8_t  active = 0;
    uint8_t  flags = 0;
  };

  HistoryBuffer(HistSample* buf, size_t cap) : buf_(buf), cap_(cap) {}

  void push(const HistSample& s) {
    if (!buf_ || !cap_) return;
    buf_[head_] = s;
    head_ = (head_ + 1) % cap_;
    if (n_ < cap_) n_++;
  }
  size_t size() const { return n_; }
  size_t capacity() const { return cap_; }
  // 0 = a legrégebbi minta
  const HistSample& at(size_t i) const { return buf_[(head_ + cap_ - n_ + i) % cap_]; }

  // A teljes tárolt időszakot legfeljebb maxBuckets részre vonja össze.
  // Átlag: hőmérséklet (csak érvényes mintákból), teljesítmény, feszültség.
  // Aktív hálózat: a legtöbbször előforduló. Visszatér: a vödrök száma; perBucket = minta/vödör.
  size_t downsample(size_t maxBuckets, Bucket* out, size_t* perBucket) const {
    if (!n_ || !maxBuckets || !out) { if (perBucket) *perBucket = 0; return 0; }
    const size_t per = (n_ + maxBuckets - 1) / maxBuckets;
    if (perBucket) *perBucket = per;
    size_t nb = 0;
    for (size_t start = 0; start < n_; start += per, ++nb) {
      const size_t end = (start + per < n_) ? start + per : n_;
      long tSum = 0, pSum = 0, vSum = 0;
      size_t tN = 0;
      unsigned cnt[4] = {0, 0, 0, 0};
      uint8_t fl = 0;
      for (size_t i = start; i < end; ++i) {
        const HistSample& s = at(i);
        if (s.tempX10 != kHistNoTemp) { tSum += s.tempX10; ++tN; }
        pSum += s.pW; vSum += s.vX10;
        cnt[s.active & 3]++;
        fl |= s.flags;
      }
      const size_t m = end - start;
      Bucket& b = out[nb];
      b.tempX10 = tN ? (int16_t)(tSum / (long)tN) : kHistNoTemp;
      b.pW = (uint16_t)(pSum / (long)m);
      b.vX10 = (uint16_t)(vSum / (long)m);
      uint8_t best = 0;
      for (uint8_t k = 1; k < 4; ++k) if (cnt[k] > cnt[best]) best = k;
      b.active = best;
      b.flags = fl;
    }
    return nb;
  }

 private:
  HistSample* buf_;
  size_t cap_, head_ = 0, n_ = 0;
};
