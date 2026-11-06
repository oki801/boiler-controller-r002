#pragma once
#include "EventBus.h"
#include "IClock.h"

// Hibák felfutó/lefutó élének követése, események kiadásával.
class ErrorTracker {
 public:
  ErrorTracker(EventBus* bus = nullptr, const IClock* clk = nullptr) : bus_(bus), clk_(clk) {}

  // Visszatér: true, ha változás történt.
  bool set(ErrorCode c, bool on, const char* detail = "") {
    const uint8_t i = static_cast<uint8_t>(c);
    if (c == ErrorCode::None || i >= static_cast<uint8_t>(ErrorCode::Count)) return false;
    const uint32_t bit = 1u << i;
    const bool was = (mask_ & bit) != 0;
    if (was == on) return false;
    mask_ = on ? (mask_ | bit) : (mask_ & ~bit);
    if (bus_) {
      Event e;
      e.type = EventType::Error;
      e.code = c;
      e.raised = on;
      e.ms   = clk_ ? clk_->millis() : 0;
      char buf[48];
      if (on) join(buf, sizeof(buf), errorInfo(c).text, detail);
      else    join(buf, sizeof(buf), "megszunt", "");
      e.setMsg(buf);
      bus_->publish(e);
    }
    return true;
  }
  bool     active(ErrorCode c) const { return (mask_ >> static_cast<uint8_t>(c)) & 1u; }
  uint32_t mask() const { return mask_; }
  // Legsúlyosabb aktív hiba szintje (Info, ha nincs).
  Severity worst() const {
    Severity w = Severity::Info;
    for (uint8_t i = 1; i < static_cast<uint8_t>(ErrorCode::Count); ++i)
      if ((mask_ >> i) & 1u) {
        Severity s = errorInfo(static_cast<ErrorCode>(i)).sev;
        if (s > w) w = s;
      }
    return w;
  }
 private:
  static void join(char* d, unsigned n, const char* text, const char* detail) {
    unsigned k = 0;
    for (; text[k] && k + 1 < n; ++k) d[k] = text[k];
    if (detail && detail[0] && k + 2 < n) {
      d[k++] = ' ';
      for (unsigned j = 0; detail[j] && k + 1 < n; ++j) d[k++] = detail[j];
    }
    d[k] = 0;
  }
  EventBus*     bus_;
  const IClock* clk_;
  uint32_t      mask_ = 0;
};
