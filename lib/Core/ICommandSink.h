#pragma once
#include <stdint.h>
#include "BoilerTypes.h"
// A web és az MQTT csak ezen keresztül adhat KÉRÉST a vezérlésnek.
// A kontaktorokhoz nincs közvetlen hozzáférésük (SF12).
class ICommandSink {
 public:
  virtual ~ICommandSink() = default;
  virtual void setMode(Mode m, int8_t forcedSrc) = 0;
  virtual void setExtOk(uint8_t src, bool ok) = 0;
  virtual void ackFault() = 0;
  virtual void reboot() = 0;
};
