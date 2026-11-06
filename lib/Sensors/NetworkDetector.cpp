#include "NetworkDetector.h"

volatile uint32_t NetworkDetector::pulses_[kSources] = {0, 0, 0};

void IRAM_ATTR NetworkDetector::isr(void* arg) { (*static_cast<volatile uint32_t*>(arg))++; }

void NetworkDetector::begin(const uint8_t pins[kSources]) {
  for (uint8_t i = 0; i < kSources; ++i) {
    pinMode(pins[i], INPUT_PULLUP);                   // lebegő bemenet = nincs impulzus = "nincs hálózat"
    attachInterruptArg(digitalPinToInterrupt(pins[i]), isr, (void*)&pulses_[i], FALLING);
  }
}

void NetworkDetector::setOffMs(uint32_t ms) { for (auto& f : f_) f.setOffMs(ms); }

void NetworkDetector::update(uint32_t nowMs) {
  for (uint8_t i = 0; i < kSources; ++i) {
    const uint32_t c = pulses_[i];
    const uint32_t d = c - last_[i];
    last_[i] = c;
    f_[i].update(nowMs, d);                           // ~10 impulzus/100 ms várható, küszöb: 3
  }
}
