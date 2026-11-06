#pragma once
#include "BoilerTypes.h"

// Célhőmérséklet-termosztát hiszterézissel (a vezérlő maga is termosztát).
// A bojler saját termosztátja és hőkioldója ettől függetlenül tartalék marad (a célnál magasabbra állítva).
//
//   T >= cél               -> nem fűt  (Reached)
//   T <= cél - hiszterézis -> fűt      (Heating)
//   a sávban               -> megtartja az előző állapotot (Heating vagy Waiting)
//   cél == 0               -> kikapcsolva (Off), a garancia sem fűt
class Thermostat {
 public:
  // Visszatér: kell-e fűteni (false = minden forrás lekapcsolva).
  bool update(float targetC, float hystC, bool tempValid, float tempC, bool guaranteeActive) {
    if (targetC <= 0.0f) { demand_ = false; phase_ = HeatPhase::Off; return false; }
    if (guaranteeActive) { demand_ = true; phase_ = HeatPhase::Heating; return true; }   // a garancia a célnál alacsonyabb célt is felülírja
    if (!tempValid) { demand_ = true; phase_ = HeatPhase::Unknown; return true; }        // nincs mérés: nem tiltunk, a bojler termosztátja véd (E01)
    if (tempC >= targetC) { demand_ = false; phase_ = HeatPhase::Reached; }
    else if (tempC <= targetC - hystC) { demand_ = true; phase_ = HeatPhase::Heating; }
    else phase_ = demand_ ? HeatPhase::Heating : HeatPhase::Waiting;
    return demand_;
  }
  HeatPhase phase() const { return phase_; }
  bool demand() const { return demand_; }
 private:
  bool demand_ = false;
  HeatPhase phase_ = HeatPhase::Off;
};
