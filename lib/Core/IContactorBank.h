#pragma once
#include <stdint.h>
// A kontaktorok absztrakciója. A kimeneti lábakat kizárólag ennek az
// interfésznek a megvalósítása (GpioContactorBank) érheti el.
class IContactorBank {
 public:
  virtual ~IContactorBank() = default;
  virtual bool    close(uint8_t src) = 0;            // false: a reteszelés tiltja
  virtual void    openAll() = 0;
  virtual bool    feedback(uint8_t src) const = 0;   // valós állapot (FBi)
  virtual uint8_t closedCount() const = 0;           // visszajelzés alapján
};
