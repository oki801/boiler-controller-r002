#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <EnergyAccumulator.h>

// Forrásonkénti energiaszámlálók tartós tárolása (NVS).
class EnergyStore {
 public:
  void begin() { prefs_.begin("energy", false); }
  void load(EnergyAccumulator& e) {
    float v[kSources];
    for (uint8_t i = 0; i < kSources; ++i) { char k[4] = {'e', (char)('0' + i), 0, 0}; v[i] = prefs_.isKey(k) ? prefs_.getFloat(k, 0.0f) : 0.0f; }
    e.load(v);
  }
  void save(const EnergyAccumulator& e) {
    float v[kSources];
    e.copyTo(v);
    for (uint8_t i = 0; i < kSources; ++i) { char k[4] = {'e', (char)('0' + i), 0, 0}; prefs_.putFloat(k, v[i]); }
  }
 private:
  Preferences prefs_;
};
