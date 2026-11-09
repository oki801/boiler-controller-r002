#pragma once
#include <stdint.h>
class TimeService { public: bool isValid() const { return true; } uint16_t minuteOfDay() const { return 7 * 60 + 42; } };
