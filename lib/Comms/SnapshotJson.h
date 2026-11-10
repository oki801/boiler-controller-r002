#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Snapshot.h>

namespace snapjson {

inline float r1(float x) { return roundf(x * 10.0f) / 10.0f; }
inline float r2(float x) { return roundf(x * 100.0f) / 100.0f; }

inline const char* heatName(HeatPhase h) {
  switch (h) {
    case HeatPhase::Off:     return "off";
    case HeatPhase::Heating: return "heating";
    case HeatPhase::Waiting: return "waiting";
    case HeatPhase::Reached: return "reached";
    default:                 return "unknown";
  }
}
inline const char* modeName(Mode m) { return m == Mode::Auto ? "auto" : m == Mode::Force ? "force" : "off"; }

// Telemetria: mérések (10 s-onként MQTT-n, a webes felületen 1 s-onként).
inline void telemetry(JsonVariant d, const Snapshot& s) {
  d["ts"] = s.iso;
  if (s.tempValid) d["temp"] = r1(s.tempC); else d["temp"] = nullptr;
  if (s.pzemValid) {
    d["v"] = r1(s.v); d["i"] = r2(s.i); d["p"] = r1(s.p); d["f"] = r1(s.f); d["pf"] = r2(s.pf);
  } else {
    d["v"] = 0; d["i"] = 0; d["p"] = 0;                 // nincs fogyasztó / nincs feszültség
  }
  d["e"] = roundf(s.eTotal * 1000.0f) / 1000.0f;
  JsonArray e = d["e_src"].to<JsonArray>();
  for (uint8_t i = 0; i < kSources; ++i) e.add(roundf(s.eSrc[i] * 1000.0f) / 1000.0f);
  d["active"] = s.active < 0 ? 0 : s.active + 1;       // 0 = egyik sem
  d["rssi"] = s.rssi;
  d["heap"] = s.heap;
  d["up"] = s.uptimeS;
  if (s.trendReady) { d["scale_dev"] = r1(s.trendDevPct); d["pnorm_dev"] = r1(s.trendPowerDevPct); }
  else { d["scale_dev"] = nullptr; d["pnorm_dev"] = nullptr; }
}

// Állapot: változáskor (retained MQTT) és a webes felülethez.
inline void state(JsonVariant d, const Snapshot& s) {
  d["fsm"] = fsmName(s.fsm);
  d["active"] = s.active < 0 ? 0 : s.active + 1;
  d["wanted"] = s.wanted < 0 ? 0 : s.wanted + 1;
  d["mode"] = modeName(s.mode);
  d["forced"] = s.forced < 0 ? 0 : s.forced + 1;
  d["manual_bypass"] = s.manualBypass;
  d["latched"] = s.latched;
  if (s.fsm == FsmState::Fault) d["fault"] = errorInfo(s.fsmError).code; else d["fault"] = nullptr;
  d["time_valid"] = s.timeValid;
  d["time_est"] = s.timeEstimated;
  d["guarantee"] = s.guaranteeActive;
  d["heat"] = heatName(s.heat);
  d["target"] = s.targetC;
  d["hyst"] = s.hystC;
  JsonArray pres = d["pres"].to<JsonArray>();
  JsonArray fb = d["fb"].to<JsonArray>();
  JsonArray src = d["src"].to<JsonArray>();
  for (uint8_t i = 0; i < kSources; ++i) {
    pres.add(s.present[i]);
    fb.add(s.feedback[i]);
    JsonObject o = src.add<JsonObject>();
    o["enabled"] = s.enabled[i]; o["ext_ok"] = s.extOk[i]; o["eligible"] = s.eligible[i];
    o["blocked"] = s.blocked[i]; o["capped"] = s.capped[i]; o["prio"] = s.priority[i];
  }
  JsonArray errs = d["errors"].to<JsonArray>();
  for (uint8_t i = 1; i < static_cast<uint8_t>(ErrorCode::Count); ++i)
    if ((s.errorMask >> i) & 1u) errs.add(errorInfo(static_cast<ErrorCode>(i)).code);
}

// Szervizmód: valós és szimulált bemenetek (a /api/status "svc" objektuma és a /service oldal).
inline void service(JsonVariant d, const Snapshot& s) {
  d["active"] = s.svc;
  d["left_s"] = s.svcLeftS;
  d["s1_auto_real"] = s.s1RealAuto;                     // a GPIO21 valós állapota (AUTO = alacsony láb)
  JsonArray pr = d["pres_real"].to<JsonArray>();
  for (uint8_t i = 0; i < kSources; ++i) pr.add(s.presReal[i]);
  if (s.tempRealValid) d["temp_real"] = r1(s.tempRealC); else d["temp_real"] = nullptr;
  if (!s.svc) return;
  JsonObject m = d["sim"].to<JsonObject>();
  m["s1"] = triName(s.sim.s1);
  JsonArray p = m["pres"].to<JsonArray>();
  JsonArray fc = m["fail_close"].to<JsonArray>();
  JsonArray sk = m["stuck"].to<JsonArray>();
  for (uint8_t i = 0; i < kSources; ++i) { p.add(triName(s.sim.pres[i])); fc.add(s.sim.failClose[i]); sk.add(s.sim.stuck[i]); }
  if (s.sim.tempOn) m["temp"] = r1(s.sim.tempC); else m["temp"] = nullptr;
}

// A /api/service/sim törzsének beolvasása. A hiányzó mező = valós / nincs hiba. Hibánál false és err szöveg.
inline bool parseSim(JsonVariant j, ServiceSim& out, const char*& err) {
  ServiceSim s;
  bool ok = true;
  if (!j["s1"].isNull()) {
    const char* v = j["s1"] | "real";
    s.s1 = triFromStr(v, &ok);
    if (!ok) { err = "s1: real|auto|manual"; return false; }
  }
  if (j["pres"].is<JsonArray>()) {
    JsonArray a = j["pres"].as<JsonArray>();
    if (a.size() != kSources) { err = "pres: 3 elem kell"; return false; }
    for (uint8_t i = 0; i < kSources; ++i) {
      const char* v = a[i] | "real";
      s.pres[i] = triFromStr(v, &ok);
      if (!ok || (v[0] == 'a' || v[0] == 'm')) { err = "pres: real|on|off"; return false; }
    }
  }
  if (!j["temp"].isNull()) {
    if (!j["temp"].is<float>()) { err = "temp: szam vagy null"; return false; }
    const float t = j["temp"].as<float>();
    if (!(t >= kSvcTempMinC && t <= kSvcTempMaxC)) { err = "temp: -10..110"; return false; }
    s.tempOn = true; s.tempC = t;
  }
  if (j["fail_close"].is<JsonArray>()) {
    JsonArray a = j["fail_close"].as<JsonArray>();
    for (uint8_t i = 0; i < kSources && i < a.size(); ++i) s.failClose[i] = a[i] | false;
  }
  if (j["stuck"].is<JsonArray>()) {
    JsonArray a = j["stuck"].as<JsonArray>();
    for (uint8_t i = 0; i < kSources && i < a.size(); ++i) s.stuck[i] = a[i] | false;
  }
  out = s;
  return true;
}

}  // namespace snapjson
