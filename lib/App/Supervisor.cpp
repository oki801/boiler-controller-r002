#include "Supervisor.h"
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
#include <pins.h>
#include <config_defaults.h>
#include <version.h>

Supervisor::Supervisor()
    : errors_(&bus_, &clock_),
      bank_(pins::K_DRV, pins::K_FB, clock_, pins::ARM_EN),
      sbank_(bank_, clock_),
      fsm_(sbank_, clock_, &bus_, &errors_),
      safety_(&errors_),
      temp_(pins::ONEWIRE),
      pzem_(Serial2, pins::PZEM_RX, pins::PZEM_TX),
      log_(time_),
      history_(time_),
      mqtt_(*this, cfg_),
      web_(cfg_, shared_, *this, log_, history_, trend_, notify_, wifi_),
      notify_(cfg_, time_),
      display_(shared_, *this, cfg_, time_) {}

// ---------------------------------------------------------------- indítás
// Az újraindulás okának olvasható neve a soros naplóhoz
static const char* resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_UNKNOWN:   return "ismeretlen (pl. USB-reset feltoltes utan)";
    case ESP_RST_POWERON:   return "bekapcsolas";
    case ESP_RST_EXT:       return "kulso reset";
    case ESP_RST_SW:        return "szoftveres";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "int-wdt";
    case ESP_RST_TASK_WDT:  return "task-wdt";
    case ESP_RST_WDT:       return "wdt";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    default:                return "egyeb";
  }
}

// Indulási információ a soros vonalra (verzió, chip, memória, beállítások)
static void printBanner(const Config& c) {
  Serial.println("----------------------------------------------");
  Serial.printf("Forditva:  %s %s\n", __DATE__, __TIME__);
  Serial.printf("Eszkoz:    %s, rev %u, %u mag, %u MHz, azonosito: %s\n", ESP.getChipModel(), (unsigned)ESP.getChipRevision(),
                (unsigned)ESP.getChipCores(), (unsigned)ESP.getCpuFreqMHz(), c.deviceId);
  Serial.printf("Flash:     %u MB, PSRAM: %u kB\n", (unsigned)(ESP.getFlashChipSize() / 1048576), (unsigned)(ESP.getPsramSize() / 1024));
  Serial.printf("Program:   %u kB, szabad hely a frissitesre: %u kB\n", (unsigned)(ESP.getSketchSize() / 1024), (unsigned)(ESP.getFreeSketchSpace() / 1024));
  Serial.printf("Memoria:   szabad %u kB, legnagyobb blokk %u kB\n", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024));
  Serial.printf("MAC:       %s\n", WiFi.macAddress().c_str());
  Serial.printf("Beallitas: cel %.0f C, hiszterezis %.1f C, garantalt minimum: %s\n", (double)c.targetC, (double)c.hystC, c.guar.enabled ? "be" : "ki");
  for (uint8_t i = 0; i < kSources; ++i)
    Serial.printf("Forras %u:  %s, %s, max %.0f C\n", (unsigned)(i + 1), c.srcName[i], c.src[i].enabled ? "engedelyezve" : "tiltva", (double)c.src[i].maxTempC);
  Serial.println("----------------------------------------------");
}

void Supervisor::begin() {
  bank_.begin();                                   // 1. minden kimenet biztonságos (nyitott) állapotba
  pinMode(pins::WDT_KICK, OUTPUT);
  pinMode(pins::LED_OK, OUTPUT);
  pinMode(pins::LED_ERR, OUTPUT);
  pinMode(pins::AUTO_SENSE, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.printf("\nBojlervezerlo %s indul\n", FW_BUILD);
  svc_.initFromBoot(esp_reset_reason(), millis());     // csak a kérést követő SZOFTVERES újraindítás után aktív
  sbank_.setSimulated(svc_.active());                  // szervizmódban a valódi kontaktor-lábak végig nyitva maradnak
  if (svc_.active()) Serial.printf("SZERVIZMOD AKTIV (%u perc): szimulalt kontaktorok, a valodi kimenetek nem mozdulnak\n", (unsigned)svc_.minutes());

  cmdQ_ = xQueueCreate(8, sizeof(Cmd));
  fsOk_ = LittleFS.begin(true);               // formatOnFail: sérült fájlrendszert újraformáz
  if (!fsOk_) Serial.println("LittleFS hiba: a napló nem elérhető, a vezerles es a web igen");
  const bool cfgOk = cfg_.begin();
  if (!cfgOk) errors_.set(ErrorCode::E12, true, "");
  energyStore_.begin();
  energyStore_.load(energy_);
  const Config c = cfg_.get();
  printBanner(c);

  // Gyakori újraindulás felismerése (E10): 3 hibás reset 10 percen belül -> zárolt biztonságos mód
  rstPrefs_.begin("boiler", false);
  uint8_t rc = rstPrefs_.getUChar("rstc", 0);
  const esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("Reset ok: %s (%d)\n", resetName(rr), (int)rr);          // minden indulásnál; a hibás okoknál lent a számláló is nő
  if (rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT ||
      rr == ESP_RST_BROWNOUT) {
    rstPrefs_.putUChar("rstc", ++rc);
    Serial.printf("Hibas reset (%d), szamlalo: %u\n", (int)rr, rc);
  }
  if (rc >= 3) fsm_.forceFault(ErrorCode::E10, true);

  net_.begin(pins::PRESENCE);
  temp_.begin();
  log_.begin();
  history_.begin();
  trend_.begin();
  bus_.subscribe(LogService::eventThunk, &log_);
  mqtt_.begin();
  bus_.subscribe(MqttService::eventThunk, &mqtt_);
  notify_.begin();
  notify_.setMuted(svc_.active());                     // szervizmódban nincs külső értesítés a szimulált hibákról
  bus_.subscribe(NotifyService::eventThunk, &notify_);
  bus_.subscribe(Display::eventThunk, &display_);     // a kijelző naplóoldalához
  time_.begin(TZ_BUDAPEST);
  wifi_.begin(c);
  web_.begin();
  display_.begin();

  Event e; e.type = EventType::System; e.ms = clock_.millis();
  char m[48]; snprintf(m, sizeof(m), "boot %s reset=%d", FW_BUILD, (int)rr);
  e.setMsg(m);
  bus_.publish(e);
  if (svc_.active()) {
    Event se; se.type = EventType::System; se.ms = clock_.millis();
    char sm[48]; snprintf(sm, sizeof(sm), "szervizmod aktiv %u perc", (unsigned)svc_.minutes());
    se.setMsg(sm);
    bus_.publish(se);
  }

  xTaskCreatePinnedToCore(controlTask, "control", 6144, this, 5, nullptr, 1);
  xTaskCreatePinnedToCore(sensorTask,  "sensor",  4096, this, 3, nullptr, 1);
  xTaskCreatePinnedToCore(netTask,     "net",     8192, this, 2, nullptr, 0);
  xTaskCreatePinnedToCore(logTask,     "log",     6144, this, 1, nullptr, 0);
}

void Supervisor::controlTask(void* p) { static_cast<Supervisor*>(p)->controlLoop(); }
void Supervisor::sensorTask(void* p)  { static_cast<Supervisor*>(p)->sensorLoop(); }
void Supervisor::netTask(void* p)     { static_cast<Supervisor*>(p)->netLoop(); }
void Supervisor::logTask(void* p)     { static_cast<Supervisor*>(p)->logLoop(); }

// ---------------------------------------------------------------- parancsok (szálbiztos kérések)
void Supervisor::postCmd(const Cmd& c) {
  if (xQueueSend(cmdQ_, &c, 0) == pdTRUE) return;
  Event e; e.type = EventType::System; e.ms = clock_.millis();
  e.setMsg("parancs eldobva");                     // tele a sor: a kérés elveszett, ezt nem szabad némán elnyelni
  bus_.publish(e);
}
void Supervisor::setMode(Mode m, int8_t f)   { Cmd c{kCmdMode, (int8_t)m, f}; postCmd(c); }
void Supervisor::setExtOk(uint8_t s, bool ok) { Cmd c{kCmdExtOk, (int8_t)s, (int8_t)ok}; postCmd(c); }
void Supervisor::ackFault()                  { Cmd c{kCmdAck, 0, 0}; postCmd(c); }
void Supervisor::reboot() {
  log_.flush();
  time_.persist();
  ESP.restart();                                   // reset: a lábak lehúzottak, a kontaktorok elengednek
}

// Szervizmód belépés/kilépés: csak a jelzőt állítja és naplóz, az újraindítást a hívó végzi (a válasz előbb kimegy).
bool Supervisor::serviceRequest(bool enter, uint16_t minutes) {
  if (enter) {
    if (minutes < kSvcMinMinutes || minutes > kSvcMaxMinutes) return false;
    ServiceMode::armNext(minutes);
  } else {
    if (!svc_.active()) return false;
    ServiceMode::clearNext();
  }
  Event e; e.type = EventType::System; e.ms = clock_.millis();
  e.setMsg(enter ? "szervizmod kerve, ujrainditas" : "szervizmod kilepes, ujrainditas");
  bus_.publish(e);
  log_.flush();
  time_.persist();
  return true;
}

bool Supervisor::serviceSim(const ServiceSim& s) { return svc_.set(s); }

void Supervisor::drainCommands() {
  Cmd c;
  while (xQueueReceive(cmdQ_, &c, 0) == pdTRUE) {
    Event e; e.type = EventType::Command; e.ms = clock_.millis();
    char m[48] = "";
    switch (c.type) {
      case kCmdMode:
        mode_ = static_cast<Mode>(c.a);
        forced_ = c.b;
        if (mode_ == Mode::Force && (forced_ < 0 || forced_ >= kSources)) { mode_ = Mode::Auto; forced_ = kNone; }
        if (mode_ != Mode::Force) forced_ = kNone;
        snprintf(m, sizeof(m), "mode=%d forced=%d", (int)mode_, forced_ + 1);
        break;
      case kCmdExtOk:
        if (c.a >= 0 && c.a < kSources) extOk_[c.a] = c.b != 0;
        snprintf(m, sizeof(m), "ext_ok[%d]=%d", c.a + 1, c.b);
        break;
      case kCmdAck:
        fsm_.ackFault();
        safety_.ack();
        if (rstPrefs_.getUChar("rstc", 0)) rstPrefs_.putUChar("rstc", 0);   // E10 nyugtázása: a számláló is nulláz
        snprintf(m, sizeof(m), "ack");
        break;
    }
    e.setMsg(m);
    bus_.publish(e);
  }
}

void Supervisor::applyConfig(const Config& c) {
  fsm_.setTiming(c.timing);
  safety_.setConfig(c.safety, c.timing.retryMs);
  net_.setOffMs(c.timing.availOffMs);
}

void Supervisor::publishSwitch(int8_t from, int8_t to, const char* reason) {
  Event e; e.type = EventType::Switch; e.ms = clock_.millis(); e.from = from; e.to = to;
  e.setMsg(reason);
  bus_.publish(e);
}

// ---------------------------------------------------------------- ControlTask (100 ms)
void Supervisor::controlLoop() {
  esp_task_wdt_init(5, true);                      // szoftveres watchdog: lefagyás -> reset
  esp_task_wdt_add(nullptr);

  Config cfg = cfg_.get();
  cfgVer_ = cfg_.version();
  applyConfig(cfg);

  TickType_t tick = xTaskGetTickCount();
  bool kick = false, wasClosed = false;
  uint32_t closedAt = 0, lastStackChkMs = 0;
  UBaseType_t minStackFree = (UBaseType_t)-1;

  for (;;) {
    vTaskDelayUntil(&tick, pdMS_TO_TICKS(kControlPeriodMs));
    const uint32_t now = clock_.millis();

    if (cfg_.version() != cfgVer_) { cfg = cfg_.get(); cfgVer_ = cfg_.version(); applyConfig(cfg); }
    drainCommands();
    net_.update(now);

    // --- szervizmód: lejárat, szimulált bemenetek, szimulált kontaktorok léptetése ---
    const bool svcOn = svc_.active();
    if (svcOn && svc_.expired(now)) {
      Event xe; xe.type = EventType::System; xe.ms = now;
      xe.setMsg("szervizmod lejart, ujrainditas");
      bus_.publish(xe);
      reboot();                                    // a jelző nincs beállítva: normál módban indul újra
    }
    ServiceSim sim;                                // alapértelmezés: minden valós
    if (svcOn) { svc_.get(sim); sbank_.sim().setFaults(sim.failClose, sim.stuck); sbank_.tick(); }
    simNow_ = sim;
    const bool s1Real = digitalRead(pins::AUTO_SENSE) == LOW;
    s1RealAuto_ = s1Real;
    const bool autoSense = simBool(sim.s1, s1Real);
    SensorData sd;
    { Lock l(sensorMtx_); sd = sensor_; }
    sensorFresh_.update(now, sd.seq);
    const bool wasStale = sensorStale_;
    sensorStale_ = sensorFresh_.stale(now, kSensorStaleMs);
    if (sensorStale_) { sd.temp.valid = false; sd.pw.valid = false; }   // lefagyott SensorTask: a régi adat nem hiteles
    tempReal_ = sd.temp;                                                // a valódi mérés (szervizoldalhoz)
    if (svcOn && sim.tempOn) { sd.temp.valid = true; sd.temp.c = sim.tempC; }   // szimulált vízhőmérséklet
    if (sensorStale_ != wasStale) {
      Event se; se.type = EventType::System; se.ms = now;
      se.setMsg(sensorStale_ ? "szenzor adat elavult" : "szenzor adat friss");
      bus_.publish(se);
    }

    // --- szenzorhibák ---
    const bool closed = sbank_.closedCount() > 0;
    if (closed && !wasClosed) closedAt = now;
    wasClosed = closed;
    errors_.set(ErrorCode::E01, sd.seq > 0 && !sd.temp.valid);
    const uint32_t ref = ((int32_t)(sd.pwLastOkMs - closedAt) > 0) ? sd.pwLastOkMs : closedAt;
    errors_.set(ErrorCode::E02, !svcOn && sd.seq > 0 && closed && (uint32_t)(now - ref) > 10000);   // szervizmódban nincs szimulált mérő
    errors_.set(ErrorCode::E09, now > 120000 && !time_.isSynced(), time_.isValid() ? "becsult" : "nincs");
    errors_.set(ErrorCode::E11, !fsOk_ || log_.storageLow(), fsOk_ ? "" : "FS");

    if (sd.seq != lastSeq_) { lastSeq_ = sd.seq; if (!svcOn) energy_.update(fsm_.active(), sd.pw.valid, sd.pw.e); }   // szervizmódban nem számol energiát

    // --- hőfokhatárok és garantált minimum ---
    capGate_.update(sd.temp.c, sd.temp.valid, cfg.src, cfg.capHystC);
    const bool wasGuar = guaranteeActive_;
    guaranteeActive_ = updateGuarantee(cfg.guar, guaranteeActive_, time_.isValid(), time_.minuteOfDay(),
                                       sd.temp.valid, sd.temp.c);
    if (guaranteeActive_ != wasGuar) {
      Event ge; ge.type = EventType::System; ge.ms = now;
      ge.setMsg(guaranteeActive_ ? "garantalt minimum: be" : "garantalt minimum: ki");
      bus_.publish(ge);
    }
    const bool heatDemand = thermo_.update(cfg.targetC, cfg.hystC, sd.temp.valid, sd.temp.c, guaranteeActive_);
    const TrendService::Summary ts = trend_.summary();
    errors_.set(ErrorCode::E15, cfg.trendEnabled && ts.scaleSuspect);
    errors_.set(ErrorCode::E16, cfg.trendEnabled && ts.elementSuspect);

    // --- szelektor bemenet ---
    const bool isActive = fsm_.state() == FsmState::Active;
    SelectorInput in;
    for (uint8_t i = 0; i < kSources; ++i) {
      in.cfg[i] = cfg.src[i];
      presReal_[i] = net_.present(i);
      const bool pr = simBool(sim.pres[i], presReal_[i]);
      if (pr != presPrev_[i]) { presPrev_[i] = pr; presSince_[i] = now; }
      in.st[i].present = pr;
      in.st[i].stableSinceMs = (sim.pres[i] == Tri::Real) ? net_.stableSinceMs(i) : presSince_[i];
      in.st[i].faulted = safety_.sourceBlocked(i, now);
      in.st[i].extOk = extOk_[i];
      in.st[i].capBlocked = capGate_.blocked(i);
    }
    in.active = isActive ? fsm_.active() : kNone;
    in.nowMs = now;
    in.activeSinceMs = activeSinceMs_;
    in.timeValid = time_.isValid();
    in.minuteOfDay = time_.minuteOfDay();
    in.weekday = time_.weekday();
    in.tAvailOnMs = cfg.timing.availOnMs;
    in.tMinDwellMs = cfg.timing.minDwellMs;
    in.preempt = cfg.preempt;
    in.guaranteeActive = guaranteeActive_;

    SafetyInput si;
    si.now = now; si.tempValid = sd.temp.valid; si.tempC = sd.temp.c;
    si.fsmActive = isActive; si.active = fsm_.active();
    si.pzemValid = sd.pw.valid && !svcOn; si.v = sd.pw.v;   // szervizmódban a valódi mérő nem védi a szimulált forrást
    safety_.update(si);

    // --- döntés ---
    int8_t wanted = kNone;
    bool openNow = false, activeOk = true;
    if (mode_ == Mode::Off || safety_.overTemp() || !heatDemand) {   // Off mód, túlhőmérséklet, vagy a termosztát nem kér fűtést
      openNow = true;
    } else if (mode_ == Mode::Force) {
      wanted = selectForced(in, forced_);
      if (in.active >= 0) activeOk = (in.active == forced_) && wanted == forced_;   // másik forrásról azonnal vált
    } else {
      wanted = selectSource(in);
      if (in.active >= 0) activeOk = activeStillOk(in);
      if (safety_.switchLimited(now) && in.active >= 0 && activeOk) wanted = in.active;   // E13
    }
    if (OtaService::busy()) wanted = (in.active >= 0 && activeOk) ? in.active : kNone;     // frissítés alatt nincs váltás
    errors_.set(ErrorCode::E14, mode_ == Mode::Auto && wanted == kNone && !openNow && fsm_.state() == FsmState::Idle);

    FsmInput fi;
    fi.wanted = wanted; fi.autoMode = autoSense; fi.activeOk = activeOk; fi.openNow = openNow;
    fsm_.update(fi);

    const int8_t failed = fsm_.consumeFailedSource();
    if (failed >= 0) safety_.markSourceFailed((uint8_t)failed, now);

    const FsmState st = fsm_.state();
    if (st == FsmState::Active && prevState_ != FsmState::Active) {
      activeSinceMs_ = now;
      safety_.noteSwitch(now);
      const char* reason = mode_ == Mode::Force ? "forced"
                         : lastActive_ < 0 ? "start"
                         : (!in.st[lastActive_].present || in.st[lastActive_].faulted) ? "lost" : "priority";
      publishSwitch(lastActive_, fsm_.active(), reason);
      lastActive_ = fsm_.active();
      energyStore_.save(energy_);
    }
    if (st == FsmState::Active && fsm_.timeInStateMs() >= SwitchoverFsm::kStableMs) safety_.noteStable((uint8_t)fsm_.active());
    prevState_ = st;

    // --- ARM / watchdog / LED ---
    const bool arm = st == FsmState::Idle || st == FsmState::DeadTime || st == FsmState::Closing ||
                     st == FsmState::Active || st == FsmState::Opening;
    sbank_.setArm(arm);                              // szervizmódban nem csinál semmit (az ARM végig alacsony)
    kick = !kick;
    digitalWrite(pins::WDT_KICK, kick);
    digitalWrite(pins::LED_OK, svcOn ? (((now / 250) & 1) ? HIGH : LOW)         // szervizmód: gyors villogás
                                     : ((st == FsmState::Active || st == FsmState::Idle) ? HIGH : LOW));
    digitalWrite(pins::LED_ERR, (st == FsmState::Fault || errors_.worst() >= Severity::Error) ? HIGH : LOW);
    esp_task_wdt_reset();

    // --- tartós mentések ---
    if ((uint32_t)(now - lastEnergySaveMs_) >= kEnergyPersistMs) { lastEnergySaveMs_ = now; energyStore_.save(energy_); }
    if ((uint32_t)(now - lastStackChkMs) >= 60000) {     // szabad stack: csak akkor jelez, ha új minimum
      lastStackChkMs = now;
      const UBaseType_t fr = uxTaskGetStackHighWaterMark(nullptr);
      if (fr < minStackFree) {
        minStackFree = fr;
        Event se; se.type = EventType::System; se.ms = now;
        char m[48]; snprintf(m, sizeof(m), "stack ctrl %u", (unsigned)fr);
        se.setMsg(m);
        bus_.publish(se);
      }
    }
    if (!rstCleared_ && now >= 600000UL) { rstPrefs_.putUChar("rstc", 0); rstCleared_ = true; }

    if ((uint32_t)(now - lastSnapMs_) >= 500) { lastSnapMs_ = now; buildSnapshot(cfg, in, now); }
  }
}

void Supervisor::buildSnapshot(const Config& c, const SelectorInput& in, uint32_t now) {
  Snapshot s;
  SensorData sd;
  { Lock l(sensorMtx_); sd = sensor_; }
  if (sensorStale_) { sd.temp.valid = false; sd.pw.valid = false; }
  s.uptimeS = now / 1000;
  s.timeValid = time_.isValid();
  s.timeEstimated = time_.estimated();
  time_.iso(s.iso, sizeof(s.iso));
  s.fsm = fsm_.state();
  s.active = (fsm_.state() == FsmState::Active) ? fsm_.active() : kNone;
  s.mode = mode_;
  s.forced = forced_;
  s.manualBypass = fsm_.state() == FsmState::ManualBypass;
  s.latched = fsm_.latched();
  s.fsmError = fsm_.error();
  s.errorMask = errors_.mask();
  for (uint8_t i = 0; i < kSources; ++i) {
    s.present[i]  = in.st[i].present;
    s.feedback[i] = sbank_.feedback(i);
    s.eligible[i] = eligible(in, i);
    s.enabled[i]  = c.src[i].enabled;
    s.extOk[i]    = extOk_[i];
    s.blocked[i]  = in.st[i].faulted;
    s.capped[i]   = in.st[i].capBlocked;
    s.priority[i] = c.src[i].priority;
    s.eSrc[i]     = energy_.srcKwh(i);
  }
  s.guaranteeActive = guaranteeActive_;
  s.heat = thermo_.phase();
  s.targetC = c.targetC; s.hystC = c.hystC;
  const TrendService::Summary tsum = trend_.summary();
  s.trendReady = tsum.ready; s.trendDevPct = tsum.devPct; s.trendPowerDevPct = tsum.powerDevPct; s.trendRuns = tsum.runs;
  s.notifyOn = c.notifyKind != 0;
  s.tempValid = sd.temp.valid; s.tempC = sd.temp.c;
  s.pzemValid = sd.pw.valid && (uint32_t)(now - sd.pwLastOkMs) < 5000;
  s.v = sd.pw.v; s.i = sd.pw.i; s.p = sd.pw.p; s.f = sd.pw.f; s.pf = sd.pw.pf;
  s.eTotal = energy_.totalKwh();
  s.heap = ESP.getFreeHeap();
  s.rssi = rssi_; s.ip = ip_; s.wifiConnected = wifiUp_; s.apMode = apMode_; s.mqttConnected = mqttUp_;
  s.defaultCreds = c.webPassHash[0] == 0;
  strlcpy(s.fw, FW_BUILD, sizeof(s.fw));
  s.svc = svc_.active();
  s.svcLeftS = svc_.leftS(now);
  s.s1RealAuto = s1RealAuto_;
  for (uint8_t i = 0; i < kSources; ++i) s.presReal[i] = presReal_[i];
  s.tempRealValid = tempReal_.valid; s.tempRealC = tempReal_.c;
  s.sim = simNow_;
  snap_ = s;
  shared_.set(s);
}

// ---------------------------------------------------------------- SensorTask (~1 s)
void Supervisor::sensorLoop() {
  for (;;) {
    const TempReading t = temp_.read();            // ~800 ms (blokkol, ezért külön task)
    const PowerReading p = pzem_.read();
    {
      Lock l(sensorMtx_);
      sensor_.temp = t;
      sensor_.pw = p;
      if (p.valid) sensor_.pwLastOkMs = millis();
      sensor_.seq++;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// ---------------------------------------------------------------- NetTask
void Supervisor::netLoop() {
  Config c = cfg_.get();
  uint32_t ver = cfg_.version();
  char ssid[33], pass[65];
  strlcpy(ssid, c.wifiSsid, sizeof(ssid));
  strlcpy(pass, c.wifiPass, sizeof(pass));
  // a soros vonalra csak a VÁLTOZÁSOK kerülnek (nem periodikus sorok)
  bool pUp = false, pAp = false, pMqtt = false, pSync = false;
  uint32_t pIp = 0;
  WifiTrial::State pTrial = WifiTrial::Idle;

  for (;;) {
    if (cfg_.version() != ver) {
      ver = cfg_.version();
      c = cfg_.get();
      if (strcmp(ssid, c.wifiSsid) || strcmp(pass, c.wifiPass)) {
        strlcpy(ssid, c.wifiSsid, sizeof(ssid));
        strlcpy(pass, c.wifiPass, sizeof(pass));
        wifi_.configure(c);
      }
    }
    wifi_.loop();
    if (wifi_.trial().state() == WifiTrial::Connected) {   // a kipróbált hálózat működik: most kerül a konfigba
      JsonDocument wd;
      wd["wifi"]["ssid"] = wifi_.trial().ssid();
      wd["wifi"]["pass"] = wifi_.trial().pass();
      char werr[64] = "";
      const bool saved = cfg_.patch(wd.as<JsonVariantConst>(), werr, sizeof(werr));
      wifi_.trialSaved(saved);
      c = cfg_.get(); ver = cfg_.version();                // a kapcsolat már él: ne induljon újracsatlakozás
      strlcpy(ssid, c.wifiSsid, sizeof(ssid));
      strlcpy(pass, c.wifiPass, sizeof(pass));
    }
    const bool up = wifi_.connected();
    wifiUp_ = up; apMode_ = wifi_.apMode(); rssi_ = wifi_.rssi(); ip_ = wifi_.ip32();
    if (up != pUp || apMode_ != pAp || ip_ != pIp) {
      if (up) Serial.printf("WiFi: csatlakozva: %s, IP %s, jel %d dBm\n", wifi_.ssid().c_str(), wifi_.staIp().c_str(), (int)rssi_);
      else if (apMode_) Serial.printf("WiFi: nincs kapcsolat, sajat hozzaferesi pont (AP), IP %s\n", wifi_.apIp().c_str());
      else Serial.println("WiFi: a kapcsolat megszakadt, ujracsatlakozas...");
      if (up && apMode_) Serial.printf("WiFi: a sajat hozzaferesi pont meg elerheto, IP %s\n", wifi_.apIp().c_str());
      pUp = up; pAp = apMode_; pIp = ip_;
    }
    if (wifi_.trial().state() != pTrial) {                 // a webes WiFi-választó állapotváltásai
      pTrial = wifi_.trial().state();
      if (pTrial == WifiTrial::Pending) Serial.printf("WiFi: halozat kiprobalasa: %s\n", wifi_.trial().ssid());
      else if (pTrial == WifiTrial::Saved) Serial.printf("WiFi: a halozat mukodik, beallitas mentve: %s\n", wifi_.trial().ssid());
      else if (pTrial == WifiTrial::Failed) Serial.printf("WiFi: kiprobalas sikertelen (%s), vissza a korabbi halozatra\n", wifi_.trial().ssid());
    }
    if (up) { ota_.begin(c); ota_.loop(); }
    mqtt_.loop(shared_.snapshot());
    mqttUp_ = mqtt_.connected();
    if (mqttUp_ != pMqtt) { Serial.printf("MQTT: %s\n", mqttUp_ ? "csatlakozva" : "a kapcsolat megszakadt"); pMqtt = mqttUp_; }
    if (time_.isSynced() && !pSync) { char iso[32]; time_.iso(iso, sizeof(iso)); Serial.printf("Ido: NTP szinkron kesz: %s\n", iso); pSync = true; }
    errors_.set(ErrorCode::E08, millis() > 60000 && (!up || (c.mqttEnabled && !mqttUp_)));
    web_.loop();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------------------------------------------------------------- LogTask (1 s)
void Supervisor::logLoop() {
  bool marked = false;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    const Snapshot s = shared_.snapshot();
    const Config c = cfg_.get();
    time_.tick();
    if (s.svc) {
      Snapshot ls = s;                             // szervizmódban a szimulált mérések nem kerülhetnek a naplóba/előzménybe/trendbe
      ls.tempValid = false; ls.pzemValid = false; ls.v = ls.i = ls.p = 0;
      log_.tick(ls, c);                            // az események naplózása így is megy
    } else {
      log_.tick(s, c);
      history_.tick(s);
      trend_.tick(s, c);
    }
    // Az új firmware 60 s stabil futás után érvényesnek jelölhető (OTA visszagörgetéshez)
    if (!marked && millis() > 60000 && s.fsm != FsmState::Fault) {
      esp_ota_mark_app_valid_cancel_rollback();
      marked = true;
    }
  }
}
