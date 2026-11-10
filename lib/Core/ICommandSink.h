#pragma once
#include <stdint.h>
#include "BoilerTypes.h"
#include "ServiceSim.h"
// A web és az MQTT csak ezen keresztül adhat KÉRÉST a vezérlésnek.
// A kontaktorokhoz nincs közvetlen hozzáférésük (SF12).
class ICommandSink {
 public:
  virtual ~ICommandSink() = default;
  virtual void setMode(Mode m, int8_t forcedSrc) = 0;
  virtual void setExtOk(uint8_t src, bool ok) = 0;
  virtual void ackFault() = 0;
  virtual void reboot() = 0;
  // Szervizmód (szárazfutás). Nem tisztán virtuális: a régi megvalósítások (pl. tesztek) változatlanul fordulnak.
  //   serviceRequest(true, perc): a KÖVETKEZŐ indulás szervizmódban lesz; a hívó utána újraindít.
  //   serviceRequest(false, 0):   kilépés; a hívó utána újraindít.
  virtual bool serviceRequest(bool enter, uint16_t minutes) { (void)enter; (void)minutes; return false; }
  // Szimulált bemenetek beállítása; false, ha a szervizmód nem aktív.
  virtual bool serviceSim(const ServiceSim& s) { (void)s; return false; }
};
