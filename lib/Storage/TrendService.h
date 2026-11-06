#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <TrendMonitor.h>
#include <Snapshot.h>
#include <Config.h>
#include <Sync.h>

// Vízkő-/fűtőelem-trendfigyelés (a TrendMonitor köré): szálbiztos, az eredményt NVS-ben őrzi.
class TrendService {
 public:
  struct Summary {
    bool baseReady = false, ready = false, scaleSuspect = false, elementSuspect = false;
    uint8_t runs = 0;
    float devPct = 0, powerDevPct = 0;
  };
  void begin();
  void tick(const Snapshot& s, const Config& c);        // LogTask, 1 s-onként
  void reset();                                          // vízkőmentesítés után
  Summary summary() const;
  String json() const;                                   // /api/trend
 private:
  mutable Mutex mtx_;
  TrendMonitor mon_;
  Preferences prefs_;
};
