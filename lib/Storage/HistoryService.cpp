#include "HistoryService.h"

bool HistoryService::begin() {
  size_t cap = kCapacity;
  HistSample* mem = nullptr;
  if (psramFound()) mem = static_cast<HistSample*>(ps_malloc(cap * sizeof(HistSample)));
  if (!mem) { cap = kCapacity / 3; mem = static_cast<HistSample*>(malloc(cap * sizeof(HistSample))); }   // PSRAM nélkül 8 óra
  if (!mem) return false;
  buf_ = new HistoryBuffer(mem, cap);
  return true;
}

void HistoryService::tick(const Snapshot& s) {
  if (!buf_) return;
  const uint32_t now = millis();
  if ((uint32_t)(now - lastMs_) < kIntervalS * 1000UL) return;
  lastMs_ = now;
  HistSample h;
  h.tempX10 = s.tempValid ? (int16_t)lroundf(s.tempC * 10.0f) : kHistNoTemp;
  h.pW = s.pzemValid ? (uint16_t)constrain(lroundf(s.p), 0L, 65535L) : 0;
  h.vX10 = s.pzemValid ? (uint16_t)constrain(lroundf(s.v * 10.0f), 0L, 65535L) : 0;
  h.active = s.active < 0 ? 0 : (uint8_t)(s.active + 1);
  h.flags = s.errorMask ? 1 : 0;
  Lock l(mtx_);
  buf_->push(h);
}

String HistoryService::json(uint16_t points) {
  JsonDocument d;
  if (points < 12) points = 12;
  if (points > 600) points = 600;
  HistoryBuffer::Bucket* b = static_cast<HistoryBuffer::Bucket*>(malloc(points * sizeof(HistoryBuffer::Bucket)));
  size_t per = 0, n = 0;
  if (b && buf_) { Lock l(mtx_); n = buf_->downsample(points, b, &per); }
  d["interval_s"] = kIntervalS;
  d["step_s"] = (uint32_t)(per * kIntervalS);
  d["n"] = (uint32_t)n;
  char iso[32]; time_.iso(iso, sizeof(iso));
  d["end"] = String(iso);
  JsonArray t = d["t"].to<JsonArray>(), p = d["p"].to<JsonArray>(), v = d["v"].to<JsonArray>(), a = d["a"].to<JsonArray>();
  for (size_t i = 0; i < n; ++i) {
    if (b[i].tempX10 == kHistNoTemp) t.add(nullptr); else t.add(b[i].tempX10 / 10.0f);
    p.add(b[i].pW); v.add(b[i].vX10 / 10.0f); a.add(b[i].active);
  }
  free(b);
  String out;
  serializeJson(d, out);
  return out;
}
