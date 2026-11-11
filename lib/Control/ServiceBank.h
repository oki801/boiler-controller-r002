#pragma once
#include <stdint.h>
#include "BoilerTypes.h"
#include "IContactorBank.h"

// A kontaktorok és az FSM közé ékelt réteg. Alapból átengedi a hívásokat a valós bankhoz.
// "Virtuális" módban (szervizmód: a szimulált S1 AUTO, a valós S1 viszont nem) a relék NEM mozdulnak:
// a zárás és a visszajelzés szimulált, így a főlogika hardver nélkül kipróbálható, és a kézi bypass
// mellett sem húzhat be kontaktor (két hálózat összekötése ellen).
// Módváltáskor a valós relék mindig NYITNAK, a virtuális állapot törlődik: csak biztonságos irányba vált.
class ServiceBank : public IContactorBank {
 public:
  explicit ServiceBank(IContactorBank& real) : real_(real) {}

  void setVirtual(bool v) {
    if (v == virt_) return;
    real_.openAll();
    vcmd_ = 0;
    virt_ = v;
  }
  bool isVirtual() const { return virt_; }

  bool close(uint8_t src) override {
    if (!virt_) return real_.close(src);
    if (src >= kSources) return false;
    if (vcmd_ && !(vcmd_ & (1u << src))) return false;           // virtuálisan is egyszerre csak egy
    vcmd_ = (uint8_t)(1u << src);
    return true;
  }
  void openAll() override { if (virt_) vcmd_ = 0; else real_.openAll(); }
  bool feedback(uint8_t src) const override {
    return virt_ ? (src < kSources && ((vcmd_ >> src) & 1u)) : real_.feedback(src);
  }
  uint8_t closedCount() const override { return virt_ ? (uint8_t)__builtin_popcount(vcmd_) : real_.closedCount(); }

 private:
  IContactorBank& real_;
  bool virt_ = false;
  uint8_t vcmd_ = 0;
};
