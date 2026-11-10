#include "ServiceMode.h"
#include <string.h>
#include <esp_attr.h>

namespace {
constexpr uint32_t kMagic = 0x53565A31;   // "SVZ1"
struct SvcRtc { uint32_t magic; uint32_t minutes; uint32_t check; };
RTC_NOINIT_ATTR SvcRtc s_rtc;             // túléli a szoftveres újraindítást, bekapcsoláskor értelmetlen tartalmú

uint32_t checkOf(uint32_t minutes) { return kMagic ^ (minutes * 2654435761u) ^ 0xA5A5A5A5u; }
}  // namespace

void ServiceMode::initFromBoot(esp_reset_reason_t rr, uint32_t nowMs) {
  const SvcRtc r = s_rtc;
  memset(&s_rtc, 0, sizeof(s_rtc));       // egyszer használatos: a következő indulás már normál
  const bool ok = rr == ESP_RST_SW && r.magic == kMagic && r.minutes >= kSvcMinMinutes &&
                  r.minutes <= kSvcMaxMinutes && r.check == checkOf(r.minutes);
  if (!ok) return;
  active_ = true;
  minutes_ = (uint16_t)r.minutes;
  deadlineMs_ = nowMs + (uint32_t)r.minutes * 60000UL;
}

void ServiceMode::armNext(uint16_t minutes) {
  if (minutes < kSvcMinMinutes) minutes = kSvcMinMinutes;
  if (minutes > kSvcMaxMinutes) minutes = kSvcMaxMinutes;
  s_rtc.magic = kMagic;
  s_rtc.minutes = minutes;
  s_rtc.check = checkOf(minutes);
}

void ServiceMode::clearNext() { memset(&s_rtc, 0, sizeof(s_rtc)); }
