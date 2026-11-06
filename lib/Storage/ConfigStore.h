#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Config.h>
#include <Sync.h>

// Konfiguráció NVS-ben (JSON), szálbiztos. A patch() részleges frissítést fogad,
// validál, majd ment. A version() változásából tudja a vezérlés, hogy újra kell olvasni.
class ConfigStore {
 public:
  bool begin();                          // false: hibás/hiányzó konfig -> alapértelmezés (E12)
  Config   get() const;
  uint32_t version() const { return version_; }
  bool patch(JsonVariantConst j, char* err, size_t n);
  void toJson(JsonDocument& d, bool secrets) const;
  bool loadedOk() const { return loadedOk_; }
 private:
  static bool applyJson(Config& c, JsonVariantConst j);
  bool save();
  mutable Mutex mtx_;
  Config   cfg_;
  Preferences prefs_;
  volatile uint32_t version_ = 1;
  bool loadedOk_ = true;
};
