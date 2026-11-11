#pragma once
#include <stdint.h>
#include "BoilerTypes.h"

// Szervizmód: szimulált bemenetek és kézi relé-kérések, hátralévő idővel.
// SOHA nem tárolódik: újraindítás után mindig kikapcsolt. Tiszta osztály (nincs hardver), a szálbiztonságról
// a hívó gondoskodik (a Supervisor zárat használ, és a vezérlő ciklus csak másolatot olvas).
enum class SimS1 : uint8_t { Real, Auto, NotAuto };
enum class SimGrid : uint8_t { Real, Present, Absent };

class ServiceMode {
 public:
  static constexpr uint32_t kDefaultTimeoutMs = 5UL * 60UL * 1000UL;   // kilépési időzítő
  static constexpr uint32_t kMinTimeoutMs = 30UL * 1000UL, kMaxTimeoutMs = 30UL * 60UL * 1000UL;
  static constexpr uint32_t kDefaultRelayMs = 10UL * 1000UL, kMaxRelayMs = 60UL * 1000UL;
  static constexpr uint32_t kRelayTestIdleMs = 60UL * 1000UL;          // ennyi tétlenség után visszaáll az automatika

  bool active() const { return active_; }

  void enter(uint32_t now, uint32_t timeoutMs = kDefaultTimeoutMs) {
    reset();
    active_ = true;
    timeoutMs_ = timeoutMs < kMinTimeoutMs ? kMinTimeoutMs : (timeoutMs > kMaxTimeoutMs ? kMaxTimeoutMs : timeoutMs);
    lastTouchMs_ = now;
  }
  void exit() { reset(); active_ = false; }
  void touch(uint32_t now) { if (active_) lastTouchMs_ = now; }      // minden szervizműveletnél újraindul

  // Időzítők léptetése. true: a szervizmód EZÉRT a hívásért járt le (kilépett).
  bool update(uint32_t now) {
    if (!active_) return false;
    if ((uint32_t)(now - lastTouchMs_) >= timeoutMs_) { exit(); return true; }
    bool any = false;
    for (uint8_t i = 0; i < kSources; ++i) {
      if (relayOn_[i] && (int32_t)(relayUntilMs_[i] - now) <= 0) relayOn_[i] = false;   // a kézi relé időkorlátja
      any = any || relayOn_[i];
    }
    if (relayTest_ && !any && (uint32_t)(now - relayTestMs_) >= kRelayTestIdleMs) relayTest_ = false;
    return false;
  }
  uint32_t remainingMs(uint32_t now) const {
    if (!active_) return 0;
    const uint32_t el = now - lastTouchMs_;
    return el >= timeoutMs_ ? 0 : timeoutMs_ - el;
  }

  // ---- szimulált bemenetek (csak aktív szervizmódban)
  bool setS1(SimS1 m, uint32_t now) { if (!active_) return false; s1_ = m; touch(now); return true; }
  bool setGrid(uint8_t i, SimGrid m, uint32_t now) {
    if (!active_ || i >= kSources) return false;
    grid_[i] = m; gridSetMs_[i] = now; touch(now); return true;
  }
  bool setTemp(bool on, float c, uint32_t now) {
    if (!active_) return false;
    tempOn_ = on; tempC_ = c; touch(now); return true;
  }
  SimS1   s1Sim() const { return s1_; }
  SimGrid gridSim(uint8_t i) const { return i < kSources ? grid_[i] : SimGrid::Real; }

  // Tényleges (felülbírált vagy valós) értékek: a főlogika EZEKET látja
  bool s1(bool real) const {
    if (!active_) return real;
    return s1_ == SimS1::Auto ? true : (s1_ == SimS1::NotAuto ? false : real);
  }
  bool grid(uint8_t i, bool real) const {
    if (!active_ || i >= kSources) return real;
    return grid_[i] == SimGrid::Present ? true : (grid_[i] == SimGrid::Absent ? false : real);
  }
  // Szimulált "jelen van": a stabilitási idő a szimuláció beállításától számít (így a rendelkezésre állási késleltetés is érvényesül)
  uint32_t gridStableSince(uint8_t i, uint32_t realSince) const {
    return (active_ && i < kSources && grid_[i] == SimGrid::Present) ? gridSetMs_[i] : realSince;
  }
  bool tempSim(float& c) const { if (active_ && tempOn_) { c = tempC_; return true; } return false; }

  // ---- kézi relé-kérés. Visszatérés: 0 rendben, 1 nem aktív, 2 rossz sorszám, 3 már be van húzva egy másik (reteszelés)
  int requestRelay(uint8_t n, bool on, uint32_t ms, bool s1Override, uint32_t now) {
    if (!active_) return 1;
    if (n >= kSources) return 2;
    touch(now);
    if (on) {
      for (uint8_t i = 0; i < kSources; ++i) if (i != n && relayOn_[i]) return 3;
      if (ms == 0) ms = kDefaultRelayMs;
      if (ms > kMaxRelayMs) ms = kMaxRelayMs;
      relayOn_[n] = true; relayReqMs_[n] = now; relayUntilMs_[n] = now + ms;
      s1Override_ = s1Override;
    } else {
      relayOn_[n] = false;
    }
    relayTest_ = true; relayTestMs_ = now;
    return 0;
  }
  bool relayWanted(uint8_t n, uint32_t now) const {
    return active_ && n < kSources && relayOn_[n] && (int32_t)(relayUntilMs_[n] - now) > 0;
  }
  uint32_t relayLeftMs(uint8_t n, uint32_t now) const {
    return relayWanted(n, now) ? relayUntilMs_[n] - now : 0;
  }
  uint32_t relayReqMs(uint8_t n) const { return n < kSources ? relayReqMs_[n] : 0; }
  bool relayTest() const { return active_ && relayTest_; }          // true: az automatika a relékről lemond (FSM Service)
  bool s1Override() const { return s1Override_; }
  void clearRelays() { for (auto& r : relayOn_) r = false; }        // védelem kioldott: minden kézi kérés törlődik
  void releaseRelays() { clearRelays(); relayTest_ = false; }

 private:
  void reset() {
    s1_ = SimS1::Real; tempOn_ = false; tempC_ = 0;
    for (uint8_t i = 0; i < kSources; ++i) { grid_[i] = SimGrid::Real; gridSetMs_[i] = 0; relayOn_[i] = false; relayReqMs_[i] = 0; relayUntilMs_[i] = 0; }
    relayTest_ = false; s1Override_ = false; relayTestMs_ = 0;
  }
  bool active_ = false;
  uint32_t timeoutMs_ = kDefaultTimeoutMs, lastTouchMs_ = 0, relayTestMs_ = 0;
  SimS1   s1_ = SimS1::Real;
  SimGrid grid_[kSources] = {SimGrid::Real, SimGrid::Real, SimGrid::Real};
  uint32_t gridSetMs_[kSources] = {0, 0, 0};
  bool    tempOn_ = false;
  float   tempC_ = 0;
  bool    relayTest_ = false, s1Override_ = false;
  bool    relayOn_[kSources] = {false, false, false};
  uint32_t relayReqMs_[kSources] = {0, 0, 0}, relayUntilMs_[kSources] = {0, 0, 0};
};

// Hibás szervizkód esetén próbálkozás-korlát: 3 hiba után 60 s zárolás.
class AttemptLimiter {
 public:
  static constexpr uint8_t  kMaxFails = 3;
  static constexpr uint32_t kLockMs = 60UL * 1000UL;
  bool locked(uint32_t now) const { return locked_ && (int32_t)(until_ - now) > 0; }
  uint32_t retryAfterMs(uint32_t now) const { return locked(now) ? until_ - now : 0; }
  void fail(uint32_t now) {
    if (locked(now)) return;
    locked_ = false;
    if (++fails_ >= kMaxFails) { locked_ = true; until_ = now + kLockMs; fails_ = 0; }
  }
  void ok() { fails_ = 0; locked_ = false; }
 private:
  uint8_t fails_ = 0; bool locked_ = false; uint32_t until_ = 0;
};
