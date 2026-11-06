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

}  // namespace snapjson
