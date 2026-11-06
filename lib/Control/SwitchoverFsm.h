#pragma once
#include <stdint.h>
#include "BoilerTypes.h"
#include "ErrorCodes.h"
#include "ErrorTracker.h"
#include "EventBus.h"
#include "IClock.h"
#include "IContactorBank.h"

struct FsmInput {
  int8_t wanted    = kNone;   // a szelektor által kért forrás
  bool   autoMode  = true;    // S1 AUTO állásban van
  bool   activeOk  = true;    // az aktív forrás még használható
  bool   openNow   = false;   // azonnali lekapcsolás (Off mód, túlhőmérséklet, OTA-hiba stb.)
};

// Váltási állapotgép: nyit -> holtidő -> zár -> visszajelzés ellenőrzés.
class SwitchoverFsm {
 public:
  static constexpr uint32_t kInitMs = 2000, kSettleMs = 500, kStableMs = 60000;
  static constexpr uint8_t  kMaxRetries = 3;

  SwitchoverFsm(IContactorBank& bank, const IClock& clk, EventBus* bus = nullptr, ErrorTracker* err = nullptr);

  void setTiming(const TimingCfg& t) { timing_ = t; }
  void update(const FsmInput& in);             // 100 ms-onként hívandó

  FsmState  state()  const { return state_; }
  int8_t    active() const { return active_; }
  ErrorCode error()  const { return error_; }
  bool      latched() const { return latched_; }
  uint32_t  timeInStateMs() const { return clk_.millis() - enteredMs_; }

  int8_t consumeFailedSource() { int8_t f = failedSource_; failedSource_ = kNone; return f; }
  void   ackFault();
  void   forceFault(ErrorCode c, bool latch) { fault(c, latch); }

 private:
  void enter(FsmState s);
  void fault(ErrorCode c, bool latch);
  void clearHwErrors();

  IContactorBank& bank_;
  const IClock&   clk_;
  EventBus*       bus_;
  ErrorTracker*   err_;
  TimingCfg       timing_;
  FsmState        state_ = FsmState::Init;
  int8_t          active_ = kNone, target_ = kNone, failedSource_ = kNone;
  ErrorCode       error_ = ErrorCode::None;
  bool            latched_ = false;
  uint8_t         retries_ = 0;
  uint32_t        enteredMs_ = 0;
};
