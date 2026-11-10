#pragma once
#include <Arduino.h>
#include <BoilerTypes.h>
#include <IClock.h>
#include <IContactorBank.h>
#include <GpioContactorBank.h>
#include <SimBank.h>

// Átkapcsoló a valódi és a szimulált kontaktor-egység között. A választás INDULÁSKOR dől el (setSimulated),
// futás közben nem változik: szervizmódba belépni és kilépni újraindítással lehet, így az állapotgép mindig
// tiszta lappal kezd. Szimulált üzemben a valódi lábakhoz (kontaktor kapuk, ARM) semmi nem nyúl.
class ServiceBank : public IContactorBank {
 public:
  ServiceBank(GpioContactorBank& real, const IClock& clk) : real_(real), sim_(clk) {}

  void setSimulated(bool on) { simulated_ = on; if (on) { real_.openAll(); real_.setArm(false); } }
  bool simulated() const { return simulated_; }

  bool close(uint8_t src) override { return simulated_ ? sim_.close(src) : real_.close(src); }
  void openAll() override { if (simulated_) sim_.openAll(); else real_.openAll(); }
  bool feedback(uint8_t src) const override { return simulated_ ? sim_.feedback(src) : real_.feedback(src); }
  uint8_t closedCount() const override { return simulated_ ? sim_.closedCount() : real_.closedCount(); }

  void setArm(bool on) { if (!simulated_) real_.setArm(on); }   // szimulált üzemben az ARM végig alacsony
  void tick() { if (simulated_) sim_.tick(); }
  SimBank& sim() { return sim_; }

 private:
  GpioContactorBank& real_;
  SimBank sim_;
  bool simulated_ = false;
};
