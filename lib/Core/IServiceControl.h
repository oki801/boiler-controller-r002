#pragma once
#include <stdint.h>
#include "BoilerTypes.h"
#include "ServiceMode.h"

// A szervizmód vezérlési felülete (a Supervisor valósítja meg, a webes API hívja). Minden hívás szálbiztos.
struct ServiceState {
  bool     active = false;
  uint32_t leftS = 0;                           // a kilépési időzítőből hátralévő másodperc
  FsmState fsm = FsmState::Init;
  bool     relayTest = false;                   // az automatika lemondott a relékről
  bool     virtualRelays = false;               // a relék NEM mozdulnak (szimulált S1 AUTO, valós nem)
  bool     s1Override = false;
  bool     s1Raw = true, s1Eff = true;          // true = AUTO
  SimS1    s1Sim = SimS1::Real;
  bool     gridRaw[kSources] = {false, false, false}, gridEff[kSources] = {false, false, false};
  SimGrid  gridSim[kSources] = {SimGrid::Real, SimGrid::Real, SimGrid::Real};
  bool     tempRealValid = false; float tempReal = 0;
  bool     tempSimOn = false;     float tempSim = 0;
  bool     relayWanted[kSources] = {false, false, false}, relayFb[kSources] = {false, false, false};
  uint32_t relayLeftS[kSources] = {0, 0, 0};
  char     msg[48] = "";                         // az utolsó szerviz-üzenet (pl. relé elutasítva)
  uint32_t uptimeS = 0, heap = 0;
  int8_t   rssi = 0;
  char     resetReason[40] = "";
};

class IServiceControl {
 public:
  virtual ~IServiceControl() = default;
  // 0 rendben, 1 hibás kód, 2 zárolva (retryS: hány másodperc múlva próbálhatja újra)
  virtual int  serviceEnter(const char* code, uint32_t& retryS) = 0;
  virtual void serviceExit() = 0;
  virtual bool serviceActive() = 0;
  virtual bool serviceState(ServiceState& out) = 0;       // false: nem aktív
  virtual bool serviceSimS1(SimS1 m) = 0;
  virtual bool serviceSimGrid(uint8_t i, SimGrid m) = 0;
  virtual bool serviceSimTemp(bool on, float c) = 0;
  // 0 sorba állítva, 1 nem aktív, 2 rossz sorszám, 3 reteszelés (másik relé be van húzva), 4 az S1 nem AUTO (s1Override kell)
  virtual int  serviceRelay(uint8_t n, bool on, uint32_t seconds, bool s1Override) = 0;
  virtual bool serviceRelease() = 0;                      // a relé-teszt vége, az automatika visszaveszi
  virtual bool serviceFactoryReset() = 0;
};
