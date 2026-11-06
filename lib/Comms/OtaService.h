#pragma once
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Config.h>

class OtaService {
 public:
  void begin(const Config& c);
  void loop() { if (started_) ArduinoOTA.handle(); }
  static bool busy() { return busy_; }
  static void setBusy(bool b) { busy_ = b; }     // a webes feltöltés is használja
 private:
  static volatile bool busy_;
  bool started_ = false;
};
