#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <Preferences.h>

#include <BoilerTypes.h>
#include <EventBus.h>
#include <ErrorTracker.h>
#include <EnergyAccumulator.h>
#include <ICommandSink.h>
#include <SourceSelector.h>
#include <SwitchoverFsm.h>
#include <SafetyMonitor.h>
#include <GuaranteeLogic.h>
#include <Thermostat.h>
#include <SensorFreshness.h>

#include <SystemClock.h>
#include <GpioContactorBank.h>
#include <ServiceBank.h>
#include <ServiceSim.h>
#include <TimeService.h>
#include <Sync.h>
#include <TempSensor.h>
#include <PowerMeter.h>
#include <NetworkDetector.h>
#include <ConfigStore.h>
#include <EnergyStore.h>
#include <LogService.h>
#include <HistoryService.h>
#include <TrendService.h>
#include <WifiManager.h>
#include <MqttService.h>
#include <WebService.h>
#include <OtaService.h>
#include <NotifyService.h>
#include <Display.h>
#include "SharedState.h"
#include "ServiceMode.h"

// Összeköti a modulokat és elindítja a FreeRTOS feladatokat.
class Supervisor : public ICommandSink {
 public:
  Supervisor();
  void begin();

  // ICommandSink: szálbiztos kérések -> a ControlTask dolgozza fel
  void setMode(Mode m, int8_t forcedSrc) override;
  void setExtOk(uint8_t src, bool ok) override;
  void ackFault() override;
  void reboot() override;
  bool serviceRequest(bool enter, uint16_t minutes) override;   // szervizmód (szárazfutás): a hívó utána újraindít
  bool serviceSim(const ServiceSim& s) override;

 private:
  struct Cmd { uint8_t type; int8_t a; int8_t b; };
  enum : uint8_t { kCmdMode, kCmdExtOk, kCmdAck };
  struct SensorData {
    TempReading temp;
    PowerReading pw;
    uint32_t pwLastOkMs = 0;
    uint32_t seq = 0;
  };

  static void controlTask(void* p);
  static void sensorTask(void* p);
  static void netTask(void* p);
  static void logTask(void* p);
  void controlLoop();
  void sensorLoop();
  void netLoop();
  void logLoop();

  void applyConfig(const Config& c);
  void drainCommands();
  void buildSnapshot(const Config& c, const SelectorInput& in, uint32_t now);
  void publishSwitch(int8_t from, int8_t to, const char* reason);
  void postCmd(const Cmd& c);                      // sorba állít; ha tele van, eseményt ír

  SystemClock   clock_;
  EventBus      bus_;
  ErrorTracker  errors_;
  ConfigStore   cfg_;
  GpioContactorBank bank_;
  ServiceBank   sbank_;        // valódi / szimulált kontaktor-egység (a döntés induláskor dől el)
  ServiceMode   svc_;
  NetworkDetector net_;
  SwitchoverFsm fsm_;
  SafetyMonitor safety_;
  EnergyAccumulator energy_;
  EnergyStore   energyStore_;
  TempSensor    temp_;
  PowerMeter    pzem_;
  TimeService   time_;
  SharedState   shared_;
  LogService    log_;
  HistoryService history_;
  TrendService  trend_;
  WifiManager   wifi_;
  MqttService   mqtt_;
  WebService    web_;
  OtaService    ota_;
  NotifyService notify_;
  CapGate       capGate_;
  Thermostat    thermo_;
  bool          guaranteeActive_ = false;
  SensorFreshness sensorFresh_;
  bool          sensorStale_ = false;
  Display       display_;

  QueueHandle_t cmdQ_ = nullptr;
  Mutex         sensorMtx_;
  SensorData    sensor_;

  // A ControlTask által birtokolt állapot
  Mode    mode_ = Mode::Auto;
  int8_t  forced_ = kNone;
  bool    extOk_[kSources] = {false, false, false};
  int8_t  lastActive_ = kNone;
  FsmState prevState_ = FsmState::Init;
  uint32_t activeSinceMs_ = 0, lastEnergySaveMs_ = 0, lastSeq_ = 0;
  uint32_t cfgVer_ = 0;
  Snapshot snap_;
  // Szervizmód: a ControlTask által birtokolt segédállapot
  bool     s1RealAuto_ = false;
  bool     presReal_[kSources] = {false, false, false};
  bool     presPrev_[kSources] = {false, false, false};
  uint32_t presSince_[kSources] = {0, 0, 0};
  TempReading tempReal_;
  ServiceSim  simNow_;
  uint32_t lastSnapMs_ = 0;
  Preferences rstPrefs_;
  bool rstCleared_ = false;
  bool fsOk_ = true;
  volatile bool wifiUp_ = false, apMode_ = false, mqttUp_ = false;
  volatile int8_t rssi_ = 0;
  volatile uint32_t ip_ = 0;
};
