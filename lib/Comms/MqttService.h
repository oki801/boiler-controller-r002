#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ICommandSink.h>
#include <EventBus.h>
#include <Snapshot.h>
#include <ConfigStore.h>
#include <freertos/queue.h>

// MQTT + Home Assistant MQTT Discovery. A parancsok csak KÉRÉSEK (ICommandSink / ConfigStore).
class MqttService {
 public:
  MqttService(ICommandSink& sink, ConfigStore& cfg) : sink_(sink), cfg_(cfg), mq_(net_) {}
  void begin();
  void loop(const Snapshot& s);                          // NetTask, ~50 ms
  bool connected() { return mq_.connected(); }
  static void eventThunk(const Event& e, void* ctx);
 private:
  bool connect(const Config& c);
  void discovery(const Config& c);
  void publishDoc(const char* component, const char* obj, JsonDocument& d);
  JsonDocument baseDoc(const char* obj, const char* name);
  void onMessage(const char* topic, const uint8_t* payload, unsigned len);
  void pub(const char* sub, const String& payload, bool retain = false);
  String topic(const char* sub) const { return base_ + "/" + sub; }

  ICommandSink& sink_;
  ConfigStore& cfg_;
  WiFiClient net_;
  PubSubClient mq_;
  QueueHandle_t q_ = nullptr;
  String base_, lastState_, id_;
  Mode lastMode_ = Mode::Auto;
  int8_t lastForced_ = 0;
  uint32_t lastAttemptMs_ = 0, backoffMs_ = 5000, lastTeleMs_ = 0, lastStateChkMs_ = 0;
  uint32_t cfgVer_ = 0;
};
