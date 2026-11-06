#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HistoryBuffer.h>
#include <Snapshot.h>
#include <TimeService.h>
#include <Sync.h>

// 24 órás előzmény (10 s-os minták) a PSRAM-ban, a webes grafikonhoz.
// Csak RAM-ban él: újraindításkor üres. A tartós napló a LittleFS-en lévő CSV.
class HistoryService {
 public:
  static constexpr uint32_t kIntervalS = 10;
  static constexpr size_t   kCapacity  = 8640;          // 24 óra
  explicit HistoryService(const TimeService& t) : time_(t) {}
  bool begin();                                         // false: nincs memória
  void tick(const Snapshot& s);                         // LogTask, 1 s-onként
  String json(uint16_t points);                         // /api/history
 private:
  const TimeService& time_;
  HistoryBuffer* buf_ = nullptr;
  Mutex mtx_;
  uint32_t lastMs_ = 0;
};
