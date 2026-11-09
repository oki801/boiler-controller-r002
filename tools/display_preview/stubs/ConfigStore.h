#pragma once
#include <Config.h>
#include <ArduinoJson.h>
class ConfigStore {
 public:
  Config cfg;
  Config get() const { return cfg; }
  bool patch(JsonVariantConst, char*, size_t) { return true; }
};
