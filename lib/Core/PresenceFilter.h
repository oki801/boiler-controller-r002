#pragma once
#include <stdint.h>

// Hálózat-jelenlét szűrő. Az optocsatolt bemenet 100 Hz-es impulzusokat ad;
// ha az ablakban elég impulzus van, a hálózat jelen van. A kiesést csak
// offMs ideig tartó impulzushiány után jelezzük (rövid kimaradás ellen).
class PresenceFilter {
 public:
  PresenceFilter(uint32_t minPulses = 3, uint32_t offMs = 2000) : minPulses_(minPulses), offMs_(offMs) {}
  void setOffMs(uint32_t v) { offMs_ = v; }
  void update(uint32_t nowMs, uint32_t pulsesInWindow) {
    if (pulsesInWindow >= minPulses_) {
      lastSeenMs_ = nowMs;
      if (!present_) { present_ = true; stableSinceMs_ = nowMs; }
    } else if (present_ && (uint32_t)(nowMs - lastSeenMs_) >= offMs_) {
      present_ = false;
    }
  }
  bool     present() const { return present_; }
  uint32_t stableSinceMs() const { return stableSinceMs_; }
 private:
  uint32_t minPulses_, offMs_;
  uint32_t lastSeenMs_ = 0, stableSinceMs_ = 0;
  bool present_ = false;
};
