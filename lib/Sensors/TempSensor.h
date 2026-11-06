#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

struct TempReading { float c = 0; bool valid = false; };

class TempSensor {
 public:
  explicit TempSensor(uint8_t pin) : ow_(pin), dt_(&ow_) {}
  void begin() { dt_.begin(); dt_.setResolution(12); dt_.setWaitForConversion(false); }
  // Blokkol ~800 ms-ig: kizárólag a SensorTask hívja.
  TempReading read();
 private:
  OneWire ow_;
  DallasTemperature dt_;
  uint8_t fails_ = 0;
  TempReading last_;
};
