#pragma once
#include <stdint.h>
#include "BoilerTypes.h"

// Szoftveres reteszelés: tiszta függvény, hardver nélkül tesztelhető.
// A GpioContactorBank minden zárás előtt ezt hívja (SF3).
struct InterlockGuard {
  static bool allowClose(uint8_t src, uint8_t commandedMask, uint8_t feedbackMask,
                         uint32_t sinceOpenMs, uint32_t minDeadMs) {
    if (src >= kSources) return false;
    if (commandedMask != 0) return false;              // már van parancsolt kontaktor
    if (feedbackMask != 0) return false;               // valamelyik még zárva (vagy összeheggedt)
    if (minDeadMs < kHardMinDeadMs) minDeadMs = kHardMinDeadMs;   // kemény alsó korlát
    if (sinceOpenMs < minDeadMs) return false;         // holtidő
    return true;
  }
};
