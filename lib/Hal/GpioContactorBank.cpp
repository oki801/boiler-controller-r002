#include "GpioContactorBank.h"

GpioContactorBank::GpioContactorBank(const uint8_t drv[kSources], const uint8_t fb[kSources],
                                     const IClock& clk, uint8_t armPin, uint32_t minDeadMs)
    : armPin_(armPin), clk_(clk), minDeadMs_(minDeadMs) {
  for (uint8_t i = 0; i < kSources; ++i) { drv_[i] = drv[i]; fb_[i] = fb[i]; }
}

void GpioContactorBank::begin() {
  pinMode(armPin_, OUTPUT);
  digitalWrite(armPin_, LOW);
  for (uint8_t i = 0; i < kSources; ++i) {
    pinMode(drv_[i], OUTPUT);
    digitalWrite(drv_[i], LOW);
    pinMode(fb_[i], INPUT_PULLUP);       // lebegő bemenet = "nyitva" (biztonságos irány)
  }
  commanded_ = 0;
  armed_ = false;
  lastOpenMs_ = clk_.millis();           // induláskor is érvényes a holtidő
}

uint8_t GpioContactorBank::feedbackMask() const {
  uint8_t m = 0;
  for (uint8_t i = 0; i < kSources; ++i) if (digitalRead(fb_[i]) == LOW) m |= (uint8_t)(1u << i);
  return m;
}

bool GpioContactorBank::feedback(uint8_t src) const {
  return src < kSources && digitalRead(fb_[src]) == LOW;
}

uint8_t GpioContactorBank::closedCount() const {
  const uint8_t m = feedbackMask();
  return (uint8_t)(((m >> 0) & 1) + ((m >> 1) & 1) + ((m >> 2) & 1));
}

bool GpioContactorBank::close(uint8_t src) {
  if (!InterlockGuard::allowClose(src, commanded_, feedbackMask(),
                                  clk_.millis() - lastOpenMs_, minDeadMs_)) return false;
  if (!armed_) return false;             // ARM nélkül nincs tekercstáp, nem is parancsolunk
  commanded_ = (uint8_t)(1u << src);
  digitalWrite(drv_[src], HIGH);
  return true;
}

void GpioContactorBank::openAll() {
  for (uint8_t i = 0; i < kSources; ++i) digitalWrite(drv_[i], LOW);
  if (commanded_) lastOpenMs_ = clk_.millis();
  commanded_ = 0;
}

void GpioContactorBank::setArm(bool on) {
  if (on == armed_) return;
  if (!on) openAll();
  digitalWrite(armPin_, on ? HIGH : LOW);
  armed_ = on;
}
