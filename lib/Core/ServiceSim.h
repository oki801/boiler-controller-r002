#pragma once
// Szervizmód (szárazfutás): szimulált bemenetek. Tiszta POD és tiszta függvények, hardver nélkül tesztelhető.
// Szervizmódban a valódi kontaktor-kimenetek NEM mozdulnak: a vezérlés egy szimulált kontaktor-egységgel
// (SimBank) dolgozik, a bemenetek (S1, hálózat-jelenlét, hőmérséklet) pedig itt felülbírálhatók.
#include <stdint.h>
#include "BoilerTypes.h"

constexpr uint16_t kSvcMinMinutes     = 1;
constexpr uint16_t kSvcMaxMinutes     = 30;
constexpr uint16_t kSvcDefaultMinutes = 10;
constexpr float    kSvcTempMinC       = -10.0f;
constexpr float    kSvcTempMaxC       = 110.0f;

enum class Tri : uint8_t { Real = 0, On = 1, Off = 2 };   // valós érték / szimulált igen / szimulált nem

struct ServiceSim {
  Tri   s1 = Tri::Real;                                   // On = AUTO állás, Off = nem AUTO (kézi)
  Tri   pres[kSources] = {Tri::Real, Tri::Real, Tri::Real};
  bool  tempOn = false;                                   // szimulált vízhőmérséklet
  float tempC = 50.0f;
  bool  failClose[kSources] = {false, false, false};      // hibainjektálás: a kontaktor nem húz be (E04)
  bool  stuck[kSources]     = {false, false, false};      // hibainjektálás: a kontaktor nem enged el (E05)
};

inline bool simBool(Tri t, bool real) { return t == Tri::Real ? real : t == Tri::On; }

inline float clampSimTemp(float c) { return c < kSvcTempMinC ? kSvcTempMinC : (c > kSvcTempMaxC ? kSvcTempMaxC : c); }

inline Tri triFromStr(const char* s, bool* ok = nullptr) {
  if (ok) *ok = true;
  if (!s || !s[0] || s[0] == 'r') return Tri::Real;       // "real"
  if (s[0] == 'o' && s[1] == 'n')  return Tri::On;        // "on"
  if (s[0] == 'o' && s[1] == 'f')  return Tri::Off;       // "off"
  if (s[0] == 'a')                 return Tri::On;        // "auto" (S1)
  if (s[0] == 'm')                 return Tri::Off;       // "manual" (S1)
  if (ok) *ok = false;
  return Tri::Real;
}
inline const char* triName(Tri t) { return t == Tri::On ? "on" : t == Tri::Off ? "off" : "real"; }
