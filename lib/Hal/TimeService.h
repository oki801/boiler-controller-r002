#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>

// Idő: NTP-ről szinkronizál, közben a belső kvarcról fut. Az időt rendszeresen NVS-be mentjük,
// így áramkimaradás után az utolsó mentett időből indul ("becsült" idő), amíg az NTP újra nem szinkronizál.
// A becsült idő hibája legfeljebb a kimaradás hossza + a mentési időköz (15 perc).
class TimeService {
 public:
  void begin(const char* tz);
  void tick();                                           // 1 s-onként hívandó
  bool isValid() const;                                  // használható idő (szinkronizált vagy becsült)
  bool isSynced() const { return synced_; }              // legalább egyszer volt NTP-szinkron ezen indítás óta
  bool estimated() const { return isValid() && !synced_; }
  void persist();                                        // azonnali mentés (pl. újraindítás előtt)
  uint16_t minuteOfDay() const;
  uint8_t  weekday() const;                              // 0 = hétfő
  void iso(char* buf, size_t n) const;                   // 2026-10-16T14:05:00+02:00
  void dateStamp(char* buf, size_t n) const;             // 20261016 vagy "nosync"
 private:
  static void onSync(struct timeval*);
  static volatile bool synced_;
  static volatile bool saveSoon_;
  Preferences prefs_;
  uint32_t lastSaveMs_ = 0;
};
