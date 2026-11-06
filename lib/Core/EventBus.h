#pragma once
#include <stdint.h>
#include <string.h>
#include "BoilerTypes.h"
#include "ErrorCodes.h"

enum class EventType : uint8_t { Switch, State, Error, Command, System };

struct Event {
  EventType type = EventType::System;
  ErrorCode code = ErrorCode::None;
  int8_t    from = kNone;
  int8_t    to   = kNone;
  uint32_t  ms   = 0;
  bool      raised = true;       // hibaeseménynél: true = fellépett, false = megszűnt
  char      msg[48] = {0};
  void setMsg(const char* s) {
    strncpy(msg, s ? s : "", sizeof(msg) - 1);
    msg[sizeof(msg) - 1] = 0;
  }
};

// Egyszerű, heap-mentes publish/subscribe. A feliratkozók szálbiztosak legyenek
// (pl. FreeRTOS queue-ba tesznek), mert a publish a hívó szálon fut.
class EventBus {
 public:
  using Handler = void (*)(const Event&, void*);
  bool subscribe(Handler h, void* ctx) {
    if (n_ >= kMax || !h) return false;
    subs_[n_++] = {h, ctx};
    return true;
  }
  void publish(const Event& e) const {
    for (uint8_t i = 0; i < n_; ++i) subs_[i].h(e, subs_[i].ctx);
  }
 private:
  static constexpr uint8_t kMax = 6;
  struct Sub { Handler h; void* ctx; };
  Sub     subs_[kMax];
  uint8_t n_ = 0;
};
