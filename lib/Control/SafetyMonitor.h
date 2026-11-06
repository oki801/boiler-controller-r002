#pragma once
#include <stdint.h>
#include "BoilerTypes.h"
#include "ErrorTracker.h"

struct SafetyInput {
  uint32_t now = 0;
  bool     tempValid = false;
  float    tempC = 0;
  bool     fsmActive = false;     // az FSM Active állapotban van
  int8_t   active = kNone;
  bool     pzemValid = false;
  float    v = 0;
};

// Másodlagos védelmek: túlhőmérséklet (E07), feszültség (E06),
// forrás-hibaszámlálás (E04 -> blokkolás), kapcsolgatás-korlát (E13).
class SafetyMonitor {
 public:
  explicit SafetyMonitor(ErrorTracker* err = nullptr) : err_(err) {}
  void setConfig(const SafetyCfg& c, uint32_t retryMs) { cfg_ = c; retryMs_ = retryMs; }

  void update(const SafetyInput& in);

  bool overTemp() const { return overTemp_; }
  bool sourceBlocked(uint8_t i, uint32_t now) const;
  bool sourceLatched(uint8_t i) const { return i < kSources && src_[i].latched; }

  void markSourceFailed(uint8_t i, uint32_t now);   // E04: kontaktor nem húzott be
  void noteStable(uint8_t i) { if (i < kSources) src_[i].fails = 0; }
  void noteSwitch(uint32_t now);
  bool switchLimited(uint32_t now) const;
  void ack();

 private:
  struct Src { bool blocked = false; uint32_t until = 0; uint8_t fails = 0; bool latched = false; };
  static constexpr uint8_t kHist = 32;

  SafetyCfg     cfg_;
  uint32_t      retryMs_ = 60000;
  ErrorTracker* err_;
  bool          overTemp_ = false;
  bool          vBad_ = false;
  uint32_t      vBadSince_ = 0;
  Src           src_[kSources];
  uint32_t      hist_[kHist] = {};
  uint8_t       histN_ = 0, histHead_ = 0;
};
