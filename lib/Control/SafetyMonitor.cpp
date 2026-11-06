#include "SafetyMonitor.h"
#include <stdio.h>

void SafetyMonitor::update(const SafetyInput& in) {
  // Túlhőmérséklet hiszterézissel
  if (in.tempValid) {
    if (!overTemp_ && in.tempC >= cfg_.tMaxC) overTemp_ = true;
    else if (overTemp_ && in.tempC <= cfg_.tMaxC - cfg_.tHystC) overTemp_ = false;
  }
  if (err_) {
    char d[16];
    snprintf(d, sizeof(d), "%.1fC", (double)in.tempC);
    err_->set(ErrorCode::E07, overTemp_, d);
  }

  // Feszültség az aktív forráson
  const bool bad = in.fsmActive && in.pzemValid && (in.v < cfg_.vMin || in.v > cfg_.vMax);
  if (bad) {
    if (!vBad_) { vBad_ = true; vBadSince_ = in.now; }
    else if ((uint32_t)(in.now - vBadSince_) >= cfg_.vBadMs && in.active >= 0 && in.active < kSources) {
      Src& s = src_[in.active];
      s.blocked = true;
      s.until = in.now + retryMs_;
      if (err_) { char d[16]; snprintf(d, sizeof(d), "%.0fV", (double)in.v); err_->set(ErrorCode::E06, true, d); }
      vBad_ = false;
    }
  } else {
    vBad_ = false;
    if (err_ && !in.fsmActive) err_->set(ErrorCode::E06, false);
  }

  // Lejárt blokkolások
  for (uint8_t i = 0; i < kSources; ++i)
    if (src_[i].blocked && (int32_t)(in.now - src_[i].until) >= 0) src_[i].blocked = false;

  if (err_) err_->set(ErrorCode::E13, switchLimited(in.now));
}

bool SafetyMonitor::sourceBlocked(uint8_t i, uint32_t now) const {
  if (i >= kSources) return true;
  const Src& s = src_[i];
  if (s.latched) return true;
  return s.blocked && (int32_t)(s.until - now) > 0;
}

void SafetyMonitor::markSourceFailed(uint8_t i, uint32_t now) {
  if (i >= kSources) return;
  Src& s = src_[i];
  s.fails++;
  if (s.fails >= cfg_.maxFailures) s.latched = true;   // csak kézi nyugtázás oldja
  s.blocked = true;
  s.until = now + retryMs_;
}

void SafetyMonitor::noteSwitch(uint32_t now) {
  hist_[histHead_] = now;
  histHead_ = (histHead_ + 1) % kHist;
  if (histN_ < kHist) histN_++;
}

bool SafetyMonitor::switchLimited(uint32_t now) const {
  uint8_t cnt = 0;
  for (uint8_t k = 0; k < histN_; ++k)
    if ((uint32_t)(now - hist_[k]) < 3600000UL) cnt++;
  return cnt >= cfg_.maxSwitchesPerHour;
}

void SafetyMonitor::ack() {
  for (uint8_t i = 0; i < kSources; ++i) src_[i] = Src();
  if (err_) { err_->set(ErrorCode::E04, false); err_->set(ErrorCode::E06, false); }
}
