#pragma once
#include <Arduino.h>
#include <BoilerTypes.h>
#include <PresenceFilter.h>

// A három hálózat jelenlétének érzékelése (100 Hz-es optocsatolt impulzusok számlálása).
class NetworkDetector {
 public:
  void begin(const uint8_t pins[kSources]);
  void setOffMs(uint32_t ms);
  void update(uint32_t nowMs);                       // 100 ms-onként
  bool     present(uint8_t i) const { return i < kSources && f_[i].present(); }
  uint32_t stableSinceMs(uint8_t i) const { return i < kSources ? f_[i].stableSinceMs() : 0; }
 private:
  static void IRAM_ATTR isr(void* arg);
  static volatile uint32_t pulses_[kSources];
  uint32_t last_[kSources] = {0, 0, 0};
  PresenceFilter f_[kSources];
};
