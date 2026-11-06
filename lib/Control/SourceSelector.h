#pragma once
#include <stdint.h>
#include "BoilerTypes.h"

// Hálózatválasztó algoritmus: tiszta függvények, nincs mellékhatás.
struct SelectorInput {
  SourceCfg    cfg[kSources];
  SourceStatus st[kSources];
  int8_t       active = kNone;       // jelenleg aktív forrás
  uint32_t     nowMs = 0, activeSinceMs = 0;
  bool         timeValid = false;
  uint16_t     minuteOfDay = 0;
  uint8_t      weekday = 0;          // 0 = hétfő
  uint32_t     tAvailOnMs = 30000, tMinDwellMs = 120000;
  bool         preempt = true;
  bool         guaranteeActive = false;   // garantált minimum aktív (lásd GuaranteeLogic.h)
};

inline bool inWindow(const SourceCfg& c, bool timeValid, uint16_t m, uint8_t wd) {
  const bool always = (c.schedFrom == c.schedTo) && ((c.dayMask & 0x7F) == 0x7F);
  if (always) return true;
  if (!timeValid) return false;                         // idő nélkül a beosztott forrás nem jogosult (E09)
  if (!(c.dayMask & (1u << (wd % 7)))) return false;
  if (c.schedFrom == c.schedTo) return true;
  return c.schedFrom < c.schedTo ? (m >= c.schedFrom && m < c.schedTo)
                                 : (m >= c.schedFrom || m < c.schedTo);
}

// Forrás használható-e egyáltalán (stabilitási idő nélkül).
inline bool usable(const SelectorInput& in, uint8_t i) {
  const SourceCfg& c = in.cfg[i]; const SourceStatus& s = in.st[i];
  if (!c.enabled || !s.present || s.faulted || s.capBlocked) return false;
  if (in.guaranteeActive && c.guarantee) return true;     // fürdővíz-garancia: beosztás és külső engedély nélkül is
  return inWindow(c, in.timeValid, in.minuteOfDay, in.weekday) && (!c.needExtOk || s.extOk);
}

// Új forrásként jelölt lehet-e (stabil jelenlét is kell).
inline bool eligible(const SelectorInput& in, uint8_t i) {
  return usable(in, i) && (uint32_t)(in.nowMs - in.st[i].stableSinceMs) >= in.tAvailOnMs;
}

// Az éppen aktív forrás maradhat-e (a stabilitási idő már nem feltétel).
inline bool activeStillOk(const SelectorInput& in) {
  return in.active >= 0 && in.active < kSources && usable(in, (uint8_t)in.active);
}

inline int8_t selectSource(const SelectorInput& in) {
  int8_t best = kNone;
  for (uint8_t i = 0; i < kSources; ++i)
    if (eligible(in, i) && (best == kNone || in.cfg[i].priority < in.cfg[best].priority)) best = (int8_t)i;

  if (in.active != kNone) {
    if (!activeStillOk(in)) return best;                // az aktív kiesett -> váltás
    const bool dwellOver = (uint32_t)(in.nowMs - in.activeSinceMs) >= in.tMinDwellMs;
    if (best != kNone && best != in.active && in.preempt && dwellOver &&
        in.cfg[best].priority < in.cfg[in.active].priority) return best;
    return in.active;                                    // marad
  }
  return best;
}

// Kényszerített forrás: az engedélyezés, jelenlét, hibamentesség és stabilitás kell,
// a beosztást és a külső engedélyt nem kerüli meg a "kényszer" (jelenlét nélkül nincs kapcsolás).
inline int8_t selectForced(const SelectorInput& in, int8_t src) {
  if (src < 0 || src >= kSources) return kNone;
  const SourceCfg& c = in.cfg[src]; const SourceStatus& s = in.st[src];
  if (!c.enabled || !s.present || s.faulted || s.capBlocked) return kNone;
  if (src != in.active && (uint32_t)(in.nowMs - s.stableSinceMs) < in.tAvailOnMs) return kNone;
  return src;
}
