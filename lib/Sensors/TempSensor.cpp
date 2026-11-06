#include "TempSensor.h"

TempReading TempSensor::read() {
  dt_.requestTemperatures();
  vTaskDelay(pdMS_TO_TICKS(800));
  const float c = dt_.getTempCByIndex(0);
  const bool ok = c != DEVICE_DISCONNECTED_C && c != 85.0f && c > -30.0f && c < 130.0f;  // 85 °C = bekapcsolási alapérték
  if (ok) { last_.c = c; last_.valid = true; fails_ = 0; }
  else if (++fails_ >= 3) last_.valid = false;     // 3 sikertelen olvasás után érvénytelen (E01)
  return last_;
}
