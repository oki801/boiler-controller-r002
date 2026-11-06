#pragma once
#include <stdint.h>
#include "BoilerTypes.h"

// A PZEM energiaszámlálójának különbségét az aktív forráshoz rendeli.
class EnergyAccumulator {
 public:
  void load(const float src[kSources]) { for (uint8_t i = 0; i < kSources; ++i) src_[i] = src[i] < 0 ? 0 : src[i]; }
  void copyTo(float out[kSources]) const { for (uint8_t i = 0; i < kSources; ++i) out[i] = src_[i]; }

  // pzemKwh: a PZEM összegző számlálója. Visszatér: a mostani többlet (kWh).
  float update(int8_t activeSrc, bool valid, float pzemKwh) {
    if (!valid) return 0;
    if (!haveLast_) { last_ = pzemKwh; haveLast_ = true; return 0; }
    float d = pzemKwh - last_;
    if (d < 0) d = (last_ > 9990.0f) ? (10000.0f - last_ + pzemKwh) : pzemKwh;  // átfordulás / nullázás
    last_ = pzemKwh;
    if (d > 50.0f) return 0;                       // nem hihető ugrás, eldobjuk
    if (activeSrc >= 0 && activeSrc < kSources) src_[activeSrc] += d;
    else other_ += d;
    return d;
  }
  float srcKwh(uint8_t i) const { return i < kSources ? src_[i] : 0; }
  float totalKwh() const { float t = other_; for (uint8_t i = 0; i < kSources; ++i) t += src_[i]; return t; }
 private:
  float src_[kSources] = {0, 0, 0};
  float other_ = 0;
  float last_ = 0;
  bool  haveLast_ = false;
};
