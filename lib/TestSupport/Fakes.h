#pragma once
// Tesztsegédek (csak natív tesztekhez).
#include <stdint.h>
#include "BoilerTypes.h"
#include "IClock.h"
#include "IContactorBank.h"
#include "InterlockGuard.h"

class FakeClock : public IClock {
 public:
  uint32_t millis() const override { return t; }
  uint32_t t = 0;
};

// Kontaktorszimuláció: késleltetett visszajelzés, hibainjektálás.
class FakeBank : public IContactorBank {
 public:
  explicit FakeBank(const FakeClock& c) : clk(c) {}

  bool close(uint8_t src) override {
    if (!InterlockGuard::allowClose(src, commanded, fbMask(), clk.millis() - lastOpen, kHardMinDeadMs)) return false;
    commanded = (uint8_t)(1u << src);
    cmdAt[src] = clk.millis();
    return true;
  }
  void openAll() override {
    for (uint8_t i = 0; i < kSources; ++i) if (commanded & (1u << i)) cmdAt[i] = clk.millis();
    commanded = 0;
    lastOpen = clk.millis();
  }
  bool feedback(uint8_t src) const override { return src < kSources && fb[src]; }
  uint8_t closedCount() const override { uint8_t n = 0; for (uint8_t i = 0; i < kSources; ++i) n += fb[i]; return n; }

  // Hardver-viselkedés léptetése (a tesztben időléptetéskor hívandó).
  void tick() {
    for (uint8_t i = 0; i < kSources; ++i) {
      const bool want = (commanded >> i) & 1u;
      const uint32_t age = clk.millis() - cmdAt[i];
      if (want && !fb[i] && !failClose[i] && age >= closeDelay) fb[i] = true;
      if (!want && fb[i] && !stuck[i] && age >= openDelay) fb[i] = false;
    }
  }
  uint8_t fbMask() const { uint8_t m = 0; for (uint8_t i = 0; i < kSources; ++i) if (fb[i]) m |= (uint8_t)(1u << i); return m; }

  const FakeClock& clk;
  uint8_t  commanded = 0;
  bool     fb[kSources] = {false, false, false};
  bool     stuck[kSources] = {false, false, false};       // összeheggedt: nem enged el
  bool     failClose[kSources] = {false, false, false};   // nem húz be
  uint32_t cmdAt[kSources] = {0, 0, 0};
  uint32_t lastOpen = 0;
  uint32_t closeDelay = 30, openDelay = 30;
};
