#include "OtaService.h"
#include <config_defaults.h>

volatile bool OtaService::busy_ = false;

void OtaService::begin(const Config& c) {
  if (started_) return;
  ArduinoOTA.setHostname(c.deviceId);
  ArduinoOTA.setPassword(c.otaPass[0] ? c.otaPass : DEFAULT_OTA_PASSWORD);
  ArduinoOTA.onStart([]() { busy_ = true; });
  ArduinoOTA.onEnd([]() { busy_ = false; });
  ArduinoOTA.onError([](ota_error_t) { busy_ = false; });
  ArduinoOTA.begin();
  started_ = true;
}
