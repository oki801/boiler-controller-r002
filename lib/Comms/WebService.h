#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ICommandSink.h>
#include <Snapshot.h>
#include <ConfigStore.h>
#include <LogService.h>
#include <HistoryService.h>
#include <TrendService.h>
#include "NotifyService.h"

// REST API + statikus webes felület (LittleFS:/www). A kezelő felület csak KÉRÉSEKET ad a vezérlésnek.
// Hitelesítés: HTTP Basic (a jelszó SHA-256 hash-e van tárolva).
class WebService {
 public:
  WebService(ConfigStore& cfg, const ISnapshotSource& snap, ICommandSink& sink, LogService& log, HistoryService& hist,
             TrendService& trend, NotifyService& notify)
      : cfg_(cfg), snap_(snap), sink_(sink), log_(log), hist_(hist), trend_(trend), notify_(notify), server_(80) {}
  void begin();
  void loop();                                       // késleltetett újraindítás kezelése
 private:
  bool authorized(AsyncWebServerRequest* r);
  void deny(AsyncWebServerRequest* r);
  void sendJson(AsyncWebServerRequest* r, JsonDocument& d, int code = 200);
  void sendErr(AsyncWebServerRequest* r, const char* msg, int code = 400);

  ConfigStore& cfg_;
  const ISnapshotSource& snap_;
  ICommandSink& sink_;
  LogService& log_;
  HistoryService& hist_;
  TrendService& trend_;
  NotifyService& notify_;
  AsyncWebServer server_;
  uint32_t restartAt_ = 0;
};
