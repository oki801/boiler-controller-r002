#pragma once
#include <Arduino.h>
#include <IClock.h>
class SystemClock : public IClock {
 public:
  uint32_t millis() const override { return ::millis(); }
};
