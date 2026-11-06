#pragma once
#include <stdint.h>
#include "BoilerTypes.h"

// Garantált minimum ("este legyen fürdéshez alkalmas víz") és forrásonkénti hőfokhatár.
// Tiszta függvények/állapot, hardver nélkül tesztelhető.

// A garancia időablaka: [readyAt - lead, until), éjfélen átnyúlhat.
inline bool inGuaranteeWindow(const GuaranteeCfg& g, uint16_t minuteOfDay) {
  const uint16_t start = (g.readyAtMin >= g.leadMin) ? (uint16_t)(g.readyAtMin - g.leadMin)
                                                      : (uint16_t)(g.readyAtMin + 1440 - g.leadMin);
  if (start == g.untilMin) return true;
  return start < g.untilMin ? (minuteOfDay >= start && minuteOfDay < g.untilMin)
                            : (minuteOfDay >= start || minuteOfDay < g.untilMin);
}

// Garancia akkor aktív, ha az ablakban a víz a minimum alatt van (hiszterézissel kapcsol ki).
// Idő vagy hőmérséklet nélkül nem aktív: vakon nem vásárolunk drága áramot (E01/E09 jelzi a hibát).
inline bool updateGuarantee(const GuaranteeCfg& g, bool wasActive, bool timeValid, uint16_t minuteOfDay,
                            bool tempValid, float tempC) {
  if (!g.enabled || !timeValid || !tempValid) return false;
  if (!inGuaranteeWindow(g, minuteOfDay)) return false;
  return wasActive ? (tempC < g.minTempC + g.hystC) : (tempC < g.minTempC);
}

// Forrásonkénti hőfokhatár hiszterézissel. Érvénytelen hőmérsékletnél nem blokkol
// (a bojler saját termosztátja véd, a hibát E01 jelzi).
class CapGate {
 public:
  void update(float tempC, bool tempValid, const SourceCfg cfg[kSources], float hystC) {
    for (uint8_t i = 0; i < kSources; ++i) {
      if (!tempValid) { blocked_[i] = false; continue; }
      if (tempC >= cfg[i].maxTempC) blocked_[i] = true;
      else if (tempC <= cfg[i].maxTempC - hystC) blocked_[i] = false;
    }
  }
  bool blocked(uint8_t i) const { return i < kSources && blocked_[i]; }
 private:
  bool blocked_[kSources] = {false, false, false};
};
