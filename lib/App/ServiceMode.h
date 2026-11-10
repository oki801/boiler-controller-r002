#pragma once
#include <Arduino.h>
#include <esp_system.h>
#include <ServiceSim.h>
#include <Sync.h>

// Szervizmód állapota. A belépés és a kilépés ÚJRAINDÍTÁSSAL történik:
//  - belépéskor a kérés (perc) az RTC-memóriába kerül, és csak az utána következő SZOFTVERES újraindítás veszi
//    figyelembe; áramszünet, watchdog, brownout vagy bármilyen más reset után az eszköz normál módban indul,
//  - a jelző egyszer használatos (az induláskor törlődik), tehát a szervizmód sosem "ragad be",
//  - lejáratkor vagy kilépéskor az eszköz újraindul normál módba.
class ServiceMode {
 public:
  // A Supervisor::begin() elején hívandó (a taskok indítása előtt).
  void initFromBoot(esp_reset_reason_t rr, uint32_t nowMs);

  bool     active() const { return active_; }               // induláskor áll be, utána nem változik
  uint16_t minutes() const { return minutes_; }
  bool     expired(uint32_t nowMs) const { return active_ && (int32_t)(nowMs - deadlineMs_) >= 0; }
  uint32_t leftS(uint32_t nowMs) const {
    if (!active_ || (int32_t)(deadlineMs_ - nowMs) <= 0) return 0;
    return (deadlineMs_ - nowMs) / 1000UL;
  }

  // Szimulált bemenetek (szálbiztos: a web írja, a vezérlő task olvassa).
  void get(ServiceSim& out) const { Lock l(m_); out = sim_; }
  bool set(const ServiceSim& in) {
    if (!active_) return false;
    ServiceSim s = in;
    s.tempC = clampSimTemp(s.tempC);
    Lock l(m_);
    sim_ = s;
    return true;
  }

  // A következő (szoftveres) indulás szervizmódban legyen / ne legyen.
  static void armNext(uint16_t minutes);
  static void clearNext();

 private:
  bool     active_ = false;
  uint16_t minutes_ = 0;
  uint32_t deadlineMs_ = 0;
  mutable Mutex m_;
  ServiceSim sim_;
};
