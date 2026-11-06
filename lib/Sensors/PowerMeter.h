#pragma once
#include <Arduino.h>
#include <PZEM004Tv30.h>

struct PowerReading {
  bool  valid = false;
  float v = 0, i = 0, p = 0, e = 0, f = 0, pf = 0;
};

// PZEM-004T V3 (Modbus RTU, UART2). Feszültség nélkül a modul nem válaszol,
// ez nem hiba, ha minden kontaktor nyitva van (a hívó dönt).
class PowerMeter {
 public:
  PowerMeter(HardwareSerial& ser, uint8_t rx, uint8_t tx) : pzem_(ser, rx, tx) {}
  PowerReading read() {
    PowerReading r;
    const float v = pzem_.voltage();
    if (isnan(v)) return r;
    r.v = v;
    r.i = nz(pzem_.current());
    r.p = nz(pzem_.power());
    r.e = nz(pzem_.energy());
    r.f = nz(pzem_.frequency());
    r.pf = nz(pzem_.pf());
    r.valid = true;
    return r;
  }
  bool resetEnergy() { return pzem_.resetEnergy(); }
 private:
  static float nz(float x) { return isnan(x) ? 0.0f : x; }
  PZEM004Tv30 pzem_;
};
