#pragma once
// Közös, hardverfüggetlen típusok. Natív (PC-s) környezetben is fordul.
#include <stdint.h>

constexpr uint8_t  kSources       = 3;
constexpr int8_t   kNone          = -1;
constexpr uint32_t kHardMinDeadMs = 2000;   // ennél rövidebb holtidő soha nem engedélyezett

enum class Mode : uint8_t { Auto = 0, Force = 1, Off = 2 };

// A termosztát fázisa (a felhasználói felület ezt mutatja)
enum class HeatPhase : uint8_t { Off, Heating, Waiting, Reached, Unknown };

enum class FsmState : uint8_t {
  Init, SelfTest, Idle, DeadTime, Closing, Active, Opening, ManualBypass, Fault
};

struct SourceCfg {
  bool     enabled   = true;
  uint8_t  priority  = 1;      // 1 = legmagasabb
  uint16_t schedFrom = 0;      // perc 0..1439
  uint16_t schedTo   = 0;      // from == to és dayMask == 0x7F -> mindig
  uint8_t  dayMask   = 0x7F;   // bit0 = hétfő ... bit6 = vasárnap
  bool     needExtOk = false;  // külső engedély (pl. napelemes többlet) kell hozzá
  float    maxTempC  = 65.0f;  // ezen a forráson csak eddig fűt (hiszterézissel újraindul)
  bool     guarantee = true;   // garantált minimum esetén beosztás/külső engedély nélkül is használható
};

// Garantált minimum: a fürdéshez alkalmas víz estére. Napközben a hidegebb víz is megfelel.
struct GuaranteeCfg {
  bool     enabled   = true;
  uint16_t readyAtMin = 19 * 60;   // eddigre legyen meg a minimum hőmérséklet
  uint16_t leadMin    = 90;        // ennyivel előbb indul a fűtés
  uint16_t untilMin   = 23 * 60;   // eddig érvényes a garancia
  float    minTempC   = 45.0f;
  float    hystC      = 3.0f;
};

struct SourceStatus {
  bool     present       = false;
  uint32_t stableSinceMs = 0;    // mióta stabilan jelen van
  bool     faulted       = false;
  bool     extOk         = false;
  bool     capBlocked    = false;  // a víz elérte ennek a forrásnak a hőfokhatárát
};

struct TimingCfg {
  uint32_t deadMs         = 5000;
  uint32_t closeTimeoutMs = 1000;
  uint32_t openTimeoutMs  = 1000;
  uint32_t minDwellMs     = 120000;
  uint32_t availOnMs      = 30000;
  uint32_t availOffMs     = 2000;
  uint32_t retryMs        = 60000;
};

struct SafetyCfg {
  float    tMaxC   = 75.0f;
  float    tHystC  = 5.0f;
  float    vMin    = 195.0f;
  float    vMax    = 253.0f;
  uint32_t vBadMs  = 5000;
  uint8_t  maxSwitchesPerHour = 20;
  uint8_t  maxFailures        = 3;
};
