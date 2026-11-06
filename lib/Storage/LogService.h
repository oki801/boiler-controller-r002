#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <EventBus.h>
#include <Snapshot.h>
#include <Config.h>
#include <TimeService.h>
#include <Sync.h>
#include <freertos/queue.h>

// CSV-napló LittleFS-en: /log/data_YYYYMMDD.csv (mérések) és /log/events.csv (események).
// RAM-puffer, 5 percenkénti flush (≥ WARN eseménynél azonnal), napi fájlok, rotáció.
class LogService {
 public:
  explicit LogService(const TimeService& t) : time_(t) {}
  bool begin();
  static void eventThunk(const Event& e, void* ctx);   // EventBus-feliratkozó (szálbiztos)
  void tick(const Snapshot& s, const Config& cfg);      // LogTask, 1 s-onként
  void flush();
  bool storageLow() const { return storageLow_; }

  String eventsTail(size_t maxBytes);
  String listFiles();                                    // JSON tömb: [{"name":..,"size":..}]
  static bool safeName(const String& n);

 private:
  static constexpr size_t kLines = 48, kLineLen = 176;
  void addDataLine(const char* line);
  void writeEvents(const char* line);
  void drainEvents();
  void prune(uint8_t retentionDays);

  const TimeService& time_;
  QueueHandle_t q_ = nullptr;
  Mutex mtx_;
  char  buf_[kLines][kLineLen];
  uint8_t n_ = 0;
  uint32_t lastSampleMs_ = 0, lastFlushMs_ = 0, lastPruneMs_ = 0;
  bool storageLow_ = false;
};
