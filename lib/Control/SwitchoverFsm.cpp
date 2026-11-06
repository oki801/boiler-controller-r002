#include "SwitchoverFsm.h"
#include "Snapshot.h"   // fsmName

SwitchoverFsm::SwitchoverFsm(IContactorBank& bank, const IClock& clk, EventBus* bus, ErrorTracker* err)
    : bank_(bank), clk_(clk), bus_(bus), err_(err) {
  enteredMs_ = clk_.millis();
  bank_.openAll();
}

void SwitchoverFsm::enter(FsmState s) {
  state_ = s;
  enteredMs_ = clk_.millis();
  if (bus_) {
    Event e;
    e.type = EventType::State;
    e.ms = enteredMs_;
    e.from = active_;
    e.to = target_;
    e.setMsg(fsmName(s));
    bus_->publish(e);
  }
}

void SwitchoverFsm::fault(ErrorCode c, bool latch) {
  bank_.openAll();
  error_ = c;
  if (latch) latched_ = true;
  active_ = kNone;
  if (err_) err_->set(c, true, "");
  enter(FsmState::Fault);
}

void SwitchoverFsm::clearHwErrors() {
  error_ = ErrorCode::None;
  if (err_) {
    err_->set(ErrorCode::E03, false);
    err_->set(ErrorCode::E05, false);
  }
}

void SwitchoverFsm::ackFault() {
  latched_ = false;
  retries_ = 0;
  if (state_ == FsmState::Fault) enter(FsmState::SelfTest);
}

void SwitchoverFsm::update(const FsmInput& in) {
  const uint32_t now = clk_.millis();
  const uint32_t inState = now - enteredMs_;      // túlcsordulás-biztos

  // Két kontaktor egyszerre zárva: azonnali, zárolt hiba.
  if (bank_.closedCount() >= 2 && state_ != FsmState::Fault) { fault(ErrorCode::E03, true); return; }

  // Kézi bypass: az automatika inaktív.
  if (!in.autoMode && state_ != FsmState::ManualBypass) {
    bank_.openAll();
    active_ = kNone;
    enter(FsmState::ManualBypass);
    return;
  }

  switch (state_) {
    case FsmState::Init:
      if (inState >= kInitMs) enter(FsmState::SelfTest);
      break;

    case FsmState::SelfTest:
      if (latched_) { bank_.openAll(); enter(FsmState::Fault); break; }
      if (bank_.closedCount() != 0) { fault(ErrorCode::E05, true); break; }
      if (inState >= kSettleMs) { clearHwErrors(); enter(FsmState::Idle); }
      break;

    case FsmState::Idle:
      if (bank_.closedCount() != 0) { fault(ErrorCode::E05, true); break; }
      if (in.wanted != kNone && !in.openNow) { target_ = in.wanted; enter(FsmState::DeadTime); }
      break;

    case FsmState::DeadTime:
      if (bank_.closedCount() != 0) { fault(ErrorCode::E05, true); break; }
      if (in.openNow || in.wanted == kNone) { enter(FsmState::Idle); break; }
      target_ = in.wanted;
      if (inState >= timing_.deadMs && bank_.close((uint8_t)target_)) enter(FsmState::Closing);
      break;

    case FsmState::Closing:
      if (bank_.feedback((uint8_t)target_) && bank_.closedCount() == 1) {
        active_ = target_;
        if (err_) err_->set(ErrorCode::E04, false);
        enter(FsmState::Active);
      } else if (inState >= timing_.closeTimeoutMs) {
        bank_.openAll();
        failedSource_ = target_;
        error_ = ErrorCode::E04;
        if (err_) err_->set(ErrorCode::E04, true, "");
        enter(FsmState::Opening);
      }
      break;

    case FsmState::Active:
      if (!bank_.feedback((uint8_t)active_)) {          // parancs nélkül elengedett
        bank_.openAll();
        failedSource_ = active_;
        error_ = ErrorCode::E04;
        if (err_) err_->set(ErrorCode::E04, true, "");
        enter(FsmState::Opening);
        break;
      }
      if (inState >= kStableMs) retries_ = 0;
      if (in.openNow || !in.activeOk ||
          (in.wanted != active_ && inState >= timing_.minDwellMs)) {
        bank_.openAll();
        enter(FsmState::Opening);
      }
      break;

    case FsmState::Opening:
      if (bank_.closedCount() == 0) {
        active_ = kNone;
        if (in.wanted != kNone && !in.openNow) { target_ = in.wanted; enter(FsmState::DeadTime); }
        else enter(FsmState::Idle);
      } else if (inState >= timing_.openTimeoutMs) {
        fault(ErrorCode::E05, true);                   // összeheggedt főérintkező
      }
      break;

    case FsmState::ManualBypass:
      if (in.autoMode) enter(FsmState::SelfTest);
      break;

    case FsmState::Fault:
      if (!latched_ && retries_ < kMaxRetries && inState >= timing_.retryMs) {
        retries_++;
        enter(FsmState::SelfTest);
      }
      break;
  }
}
