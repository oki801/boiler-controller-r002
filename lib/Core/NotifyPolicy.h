#pragma once
#include <stdint.h>
#include "ErrorCodes.h"

// Mikor kell értesítést küldeni: súlyossági küszöb, ismétlés-védelem (kapcsolgató hibánál),
// és "megszűnt" üzenet csak olyan hibára, amelyről a fellépést is jeleztük.
class NotifyPolicy {
 public:
  enum class Action : uint8_t { None, SendRaised, SendCleared };
  static constexpr uint32_t kCooldownMs = 15UL * 60UL * 1000UL;

  Action decide(ErrorCode c, bool raised, Severity minSev, uint32_t nowMs) {
    const uint8_t i = static_cast<uint8_t>(c);
    if (c == ErrorCode::None || i >= static_cast<uint8_t>(ErrorCode::Count)) return Action::None;
    if (errorInfo(c).sev < minSev) return Action::None;
    if (raised) {
      if (ever_[i] && (uint32_t)(nowMs - lastMs_[i]) < kCooldownMs) return Action::None;
      ever_[i] = true; open_[i] = true; lastMs_[i] = nowMs;
      return Action::SendRaised;
    }
    if (open_[i]) { open_[i] = false; return Action::SendCleared; }
    return Action::None;
  }
 private:
  bool     ever_[32] = {};
  bool     open_[32] = {};
  uint32_t lastMs_[32] = {};
};
