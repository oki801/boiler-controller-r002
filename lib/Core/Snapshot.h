#pragma once
// A rendszer pillanatképe (web, MQTT, napló használja). Tiszta POD.
#include <stdint.h>
#include "BoilerTypes.h"
#include "ErrorCodes.h"

struct Snapshot {
  uint32_t  uptimeS = 0;
  bool      timeValid = false;       // használható idő (szinkronizált vagy becsült)
  bool      timeEstimated = false;   // NVS-ből visszaállított idő, NTP még nem szinkronizált
  char      iso[32] = {0};

  FsmState  fsm = FsmState::Init;
  int8_t    active = kNone;
  int8_t    wanted = kNone;
  Mode      mode = Mode::Auto;
  int8_t    forced = kNone;
  bool      manualBypass = false;
  bool      latched = false;
  ErrorCode fsmError = ErrorCode::None;
  uint32_t  errorMask = 0;

  bool      present[kSources]  = {};
  bool      feedback[kSources] = {};
  bool      eligible[kSources] = {};
  bool      enabled[kSources]  = {};
  bool      extOk[kSources]    = {};
  bool      blocked[kSources]  = {};
  bool      capped[kSources]   = {};     // hőfokhatár miatt tiltva
  bool      guaranteeActive = false;     // garantált minimum éppen fűt
  HeatPhase heat = HeatPhase::Off;       // termosztát fázisa
  float     targetC = 0, hystC = 0;      // célhőmérséklet és hiszterézis
  uint8_t   priority[kSources] = {};

  bool      tempValid = false;
  float     tempC = 0;
  bool      pzemValid = false;
  float     v = 0, i = 0, p = 0, f = 0, pf = 0;
  float     eTotal = 0;
  float     eSrc[kSources] = {};

  bool      trendReady = false;
  float     trendDevPct = 0, trendPowerDevPct = 0;
  uint16_t  trendRuns = 0;
  bool      notifyOn = false;

  uint32_t  heap = 0;
  int8_t    rssi = 0;
  bool      wifiConnected = false;
  bool      apMode = false;
  bool      mqttConnected = false;
  bool      defaultCreds = false;
  char      fw[24] = {0};
};

class ISnapshotSource {
 public:
  virtual ~ISnapshotSource() = default;
  virtual Snapshot snapshot() const = 0;
};

inline const char* fsmName(FsmState s) {
  switch (s) {
    case FsmState::Init:         return "INIT";
    case FsmState::SelfTest:     return "SELFTEST";
    case FsmState::Idle:         return "IDLE";
    case FsmState::DeadTime:     return "DEAD_TIME";
    case FsmState::Closing:      return "CLOSING";
    case FsmState::Active:       return "ACTIVE";
    case FsmState::Opening:      return "OPENING";
    case FsmState::ManualBypass: return "MANUAL";
    case FsmState::Fault:        return "FAULT";
  }
  return "?";
}
