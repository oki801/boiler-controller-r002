#pragma once
// Szimulált kontaktor-egység a szervizmódhoz (késleltetett visszajelzés, hibainjektálás).
// Pontosan olyan reteszelést használ, mint a valódi egység (InterlockGuard), de lábakhoz nem nyúl.
#include <stdint.h>
#include "BoilerTypes.h"
#include "IClock.h"
#include "IContactorBank.h"
#include "InterlockGuard.h"

class SimBank : public IContactorBank {
 public:
  explicit SimBank(const IClock& c) : clk_(c) {}

  bool close(uint8_t src) override {
    if (!InterlockGuard::allowClose(src, commanded_, fbMask(), clk_.millis() - lastOpen_, kHardMinDeadMs)) return false;
    commanded_ = (uint8_t)(1u << src);
    cmdAt_[src] = clk_.millis();
    return true;
  }
  void openAll() override {
    for (uint8_t i = 0; i < kSources; ++i) if (commanded_ & (1u << i)) cmdAt_[i] = clk_.millis();
    if (commanded_) lastOpen_ = clk_.millis();
    commanded_ = 0;
  }
  bool feedback(uint8_t src) const override { return src < kSources && fb_[src]; }
  uint8_t closedCount() const override { uint8_t n = 0; for (uint8_t i = 0; i < kSources; ++i) n += fb_[i] ? 1 : 0; return n; }

  void setFaults(const bool failClose[kSources], const bool stuck[kSources]) {
    for (uint8_t i = 0; i < kSources; ++i) { failClose_[i] = failClose[i]; stuck_[i] = stuck[i]; }
  }
  // A "hardver" léptetése: a vezérlő ciklus (100 ms) hívja.
  void tick() {
    for (uint8_t i = 0; i < kSources; ++i) {
      const bool want = (commanded_ >> i) & 1u;
      const uint32_t age = clk_.millis() - cmdAt_[i];
      if (want && !fb_[i] && !failClose_[i] && age >= kCloseDelayMs) fb_[i] = true;
      if (!want && fb_[i] && !stuck_[i] && age >= kOpenDelayMs) fb_[i] = false;
    }
  }
  uint8_t fbMask() const { uint8_t m = 0; for (uint8_t i = 0; i < kSources; ++i) if (fb_[i]) m |= (uint8_t)(1u << i); return m; }

 private:
  static constexpr uint32_t kCloseDelayMs = 30, kOpenDelayMs = 30;
  const IClock& clk_;
  uint8_t  commanded_ = 0;
  bool     fb_[kSources] = {false, false, false};
  bool     failClose_[kSources] = {false, false, false};
  bool     stuck_[kSources] = {false, false, false};
  uint32_t cmdAt_[kSources] = {0, 0, 0};
  uint32_t lastOpen_ = 0;
};
