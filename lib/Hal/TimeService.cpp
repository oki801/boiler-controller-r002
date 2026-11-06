#include "TimeService.h"
#include <esp_sntp.h>

namespace { constexpr time_t kMinEpoch = 1700000000L; constexpr uint32_t kSaveEveryMs = 15UL * 60UL * 1000UL; }

volatile bool TimeService::synced_ = false;
volatile bool TimeService::saveSoon_ = false;

void TimeService::onSync(struct timeval*) { synced_ = true; saveSoon_ = true; }

void TimeService::begin(const char* tz) {
  prefs_.begin("time", false);
  // Ha nincs érvényes idő (hidegindítás), az utolsó mentett időből indulunk.
  const uint32_t saved = prefs_.getULong("epoch", 0);
  if (time(nullptr) < kMinEpoch && saved > (uint32_t)kMinEpoch) {
    struct timeval tv = {(time_t)saved, 0};
    settimeofday(&tv, nullptr);
  }
  sntp_set_time_sync_notification_cb(onSync);
  configTzTime(tz, "pool.ntp.org", "time.google.com", "hu.pool.ntp.org");
  lastSaveMs_ = millis();
}

bool TimeService::isValid() const { return time(nullptr) > kMinEpoch; }

void TimeService::persist() {
  const time_t now = time(nullptr);
  if (now > kMinEpoch) prefs_.putULong("epoch", (uint32_t)now);
  lastSaveMs_ = millis();
  saveSoon_ = false;
}

void TimeService::tick() {
  if (saveSoon_ || (uint32_t)(millis() - lastSaveMs_) >= kSaveEveryMs) persist();   // szinkron után azonnal, egyébként 15 percenként
}

static bool localNow(struct tm& t) {
  time_t now = time(nullptr);
  if (now < kMinEpoch) return false;
  localtime_r(&now, &t);
  return true;
}

uint16_t TimeService::minuteOfDay() const {
  struct tm t;
  return localNow(t) ? (uint16_t)(t.tm_hour * 60 + t.tm_min) : 0;
}

uint8_t TimeService::weekday() const {
  struct tm t;
  return localNow(t) ? (uint8_t)((t.tm_wday + 6) % 7) : 0;
}

void TimeService::iso(char* buf, size_t n) const {
  struct tm t;
  if (!localNow(t)) { snprintf(buf, n, "T+%lus", (unsigned long)(millis() / 1000)); return; }
  char z[8];
  strftime(z, sizeof(z), "%z", &t);                      // +0200
  snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d%.3s:%.2s", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
           t.tm_hour, t.tm_min, t.tm_sec, z, z + 3);
}

void TimeService::dateStamp(char* buf, size_t n) const {
  struct tm t;
  if (!localNow(t)) { snprintf(buf, n, "nosync"); return; }
  snprintf(buf, n, "%04d%02d%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}
