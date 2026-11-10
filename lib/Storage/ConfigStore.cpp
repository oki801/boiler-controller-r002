#include "ConfigStore.h"
#include <Sha256Hex.h>

namespace {
template <typename T> void rd(JsonVariantConst v, T& dst) { if (!v.isNull() && v.is<T>()) dst = v.as<T>(); }
void rs(JsonVariantConst v, char* dst, size_t n) {
  if (v.is<const char*>()) strlcpy(dst, v.as<const char*>(), n);
}
}

bool ConfigStore::applyJson(Config& c, JsonVariantConst j) {
  rs(j["deviceId"], c.deviceId, sizeof(c.deviceId));
  rd(j["preempt"], c.preempt);
  rd(j["logIntervalS"], c.logIntervalS);
  rd(j["logRetentionDays"], c.logRetentionDays);

  JsonVariantConst t = j["timing"];
  rd(t["deadMs"], c.timing.deadMs);          rd(t["closeMs"], c.timing.closeTimeoutMs);
  rd(t["openMs"], c.timing.openTimeoutMs);   rd(t["dwellMs"], c.timing.minDwellMs);
  rd(t["availOnMs"], c.timing.availOnMs);    rd(t["availOffMs"], c.timing.availOffMs);
  rd(t["retryMs"], c.timing.retryMs);

  JsonVariantConst s = j["safety"];
  rd(s["tMaxC"], c.safety.tMaxC);            rd(s["tHystC"], c.safety.tHystC);
  rd(s["vMin"], c.safety.vMin);              rd(s["vMax"], c.safety.vMax);
  rd(s["vBadMs"], c.safety.vBadMs);          rd(s["maxSwitchesPerHour"], c.safety.maxSwitchesPerHour);
  rd(s["maxFailures"], c.safety.maxFailures);

  JsonArrayConst arr = j["src"].as<JsonArrayConst>();
  uint8_t i = 0;
  for (JsonVariantConst e : arr) {
    if (i >= kSources) break;
    rs(e["name"], c.srcName[i], sizeof(c.srcName[i]));
    rd(e["enabled"], c.src[i].enabled);      rd(e["priority"], c.src[i].priority);
    rd(e["from"], c.src[i].schedFrom);       rd(e["to"], c.src[i].schedTo);
    rd(e["days"], c.src[i].dayMask);         rd(e["needExt"], c.src[i].needExtOk);
    rd(e["maxTemp"], c.src[i].maxTempC);     rd(e["guarantee"], c.src[i].guarantee);
    ++i;
  }

  JsonVariantConst g = j["guarantee"];
  rd(g["enabled"], c.guar.enabled);   rd(g["readyAt"], c.guar.readyAtMin);  rd(g["lead"], c.guar.leadMin);
  rd(g["until"], c.guar.untilMin);    rd(g["minTemp"], c.guar.minTempC);    rd(g["hyst"], c.guar.hystC);
  rd(j["capHystC"], c.capHystC);
  rd(j["target"], c.targetC);         rd(j["hyst"], c.hystC);
  JsonVariantConst nt = j["notify"];
  rd(nt["kind"], c.notifyKind);       rd(nt["minSev"], c.notifyMinSev);
  rs(nt["url"], c.notifyUrl, sizeof(c.notifyUrl));
  rs(nt["token"], c.notifyToken, sizeof(c.notifyToken));
  rs(nt["chat"], c.notifyChat, sizeof(c.notifyChat));
  rd(j["trend"]["enabled"], c.trendEnabled);  rd(j["trend"]["thresholdPct"], c.trendThresholdPct);
  rs(j["wifi"]["ssid"], c.wifiSsid, sizeof(c.wifiSsid));
  rs(j["wifi"]["pass"], c.wifiPass, sizeof(c.wifiPass));
  rd(j["mqtt"]["enabled"], c.mqttEnabled);   rs(j["mqtt"]["host"], c.mqttHost, sizeof(c.mqttHost));
  rd(j["mqtt"]["port"], c.mqttPort);         rs(j["mqtt"]["user"], c.mqttUser, sizeof(c.mqttUser));
  rs(j["mqtt"]["pass"], c.mqttPass, sizeof(c.mqttPass));
  rs(j["mqtt"]["base"], c.mqttBase, sizeof(c.mqttBase));
  rs(j["web"]["user"], c.webUser, sizeof(c.webUser));
  rs(j["ota"]["pass"], c.otaPass, sizeof(c.otaPass));
  if (j["web"]["passHash"].is<const char*>()) rs(j["web"]["passHash"], c.webPassHash, sizeof(c.webPassHash));
  if (j["web"]["pass"].is<const char*>()) {                         // nyers jelszó -> hash
    const char* pw = j["web"]["pass"].as<const char*>();
    if (pw[0]) sha256Hex(pw, c.webPassHash);
  }
  return true;
}

bool ConfigStore::begin() {
  prefs_.begin("boiler", false);
  String s = prefs_.isKey("cfg") ? prefs_.getString("cfg", "") : String();   // friss flash: nincs mentett konfig, az alapértékek maradnak
  Config c;
  bool ok = true;
  if (s.length()) {
    JsonDocument d;
    char err[64];
    if (deserializeJson(d, s) == DeserializationError::Ok) {
      applyJson(c, d.as<JsonVariantConst>());
      ok = validateConfig(c, err, sizeof(err));
    } else ok = false;
  }
  if (!ok) c = Config();                                            // alapértelmezés (E12)
  Lock l(mtx_);
  cfg_ = c;
  loadedOk_ = ok;
  return ok;
}

Config ConfigStore::get() const { Lock l(mtx_); return cfg_; }

void ConfigStore::toJson(JsonDocument& d, bool secrets) const {
  const Config c = get();      // a zár csak a másolás idejére kell
  toJsonCfg(c, d, secrets);
}

// A `c` nem feltétlenül él addig, amíg a hívó a JSON-t kiírja. Az ArduinoJson a `const char[]` szövegeket
// mutatóként tárolná (nem másolja), ezért minden szöveget String-gé alakítunk, ami másolódik. Enélkül a
// /api/config és a mentett konfiguráció szemetet tartalmaz.
// Zárat nem vesz (a patch() a zár alatt hívja): nem rekurzív mutexnél a get() ott holtpontot okozna.
void ConfigStore::toJsonCfg(const Config& c, JsonDocument& d, bool secrets) {
  d["deviceId"] = String(c.deviceId);  d["preempt"] = c.preempt;
  d["logIntervalS"] = c.logIntervalS;  d["logRetentionDays"] = c.logRetentionDays;
  JsonObject t = d["timing"].to<JsonObject>();
  t["deadMs"] = c.timing.deadMs;  t["closeMs"] = c.timing.closeTimeoutMs;  t["openMs"] = c.timing.openTimeoutMs;
  t["dwellMs"] = c.timing.minDwellMs;  t["availOnMs"] = c.timing.availOnMs;
  t["availOffMs"] = c.timing.availOffMs;  t["retryMs"] = c.timing.retryMs;
  JsonObject s = d["safety"].to<JsonObject>();
  s["tMaxC"] = c.safety.tMaxC;  s["tHystC"] = c.safety.tHystC;  s["vMin"] = c.safety.vMin;
  s["vMax"] = c.safety.vMax;  s["vBadMs"] = c.safety.vBadMs;
  s["maxSwitchesPerHour"] = c.safety.maxSwitchesPerHour;  s["maxFailures"] = c.safety.maxFailures;
  JsonArray a = d["src"].to<JsonArray>();
  for (uint8_t i = 0; i < kSources; ++i) {
    JsonObject o = a.add<JsonObject>();
    o["name"] = String(c.srcName[i]);  o["enabled"] = c.src[i].enabled;  o["priority"] = c.src[i].priority;
    o["from"] = c.src[i].schedFrom;  o["to"] = c.src[i].schedTo;  o["days"] = c.src[i].dayMask;
    o["needExt"] = c.src[i].needExtOk;
    o["maxTemp"] = c.src[i].maxTempC;  o["guarantee"] = c.src[i].guarantee;
  }
  d["capHystC"] = c.capHystC;
  d["target"] = c.targetC;  d["hyst"] = c.hystC;
  JsonObject g = d["guarantee"].to<JsonObject>();
  g["enabled"] = c.guar.enabled;  g["readyAt"] = c.guar.readyAtMin;  g["lead"] = c.guar.leadMin;
  g["until"] = c.guar.untilMin;   g["minTemp"] = c.guar.minTempC;    g["hyst"] = c.guar.hystC;
  JsonObject nt = d["notify"].to<JsonObject>();
  nt["kind"] = c.notifyKind;  nt["minSev"] = c.notifyMinSev;  nt["chat"] = String(c.notifyChat);
  nt["configured"] = c.notifyKind == 1 ? (c.notifyToken[0] && c.notifyChat[0]) : (c.notifyUrl[0] != 0);
  JsonObject tr = d["trend"].to<JsonObject>();
  tr["enabled"] = c.trendEnabled;  tr["thresholdPct"] = c.trendThresholdPct;
  JsonObject w = d["wifi"].to<JsonObject>();  w["ssid"] = String(c.wifiSsid);
  JsonObject m = d["mqtt"].to<JsonObject>();
  m["enabled"] = c.mqttEnabled;  m["host"] = String(c.mqttHost);  m["port"] = c.mqttPort;
  m["user"] = String(c.mqttUser);  m["base"] = String(c.mqttBase);
  d["web"]["user"] = String(c.webUser);
  if (secrets) {                                   // csak belső mentéshez
    w["pass"] = String(c.wifiPass);  m["pass"] = String(c.mqttPass);
    d["web"]["passHash"] = String(c.webPassHash);  d["ota"]["pass"] = String(c.otaPass);
    nt["url"] = String(c.notifyUrl);  nt["token"] = String(c.notifyToken);
  }
}

bool ConfigStore::save(const Config& c) {
  JsonDocument d;
  toJsonCfg(c, d, true);
  String s;
  serializeJson(d, s);
  return prefs_.putString("cfg", s) > 0;
}

bool ConfigStore::patch(JsonVariantConst j, char* err, size_t n) {
  Lock l(mtx_);
  Config c = cfg_;
  applyJson(c, j);
  if (!validateConfig(c, err, n)) return false;
  // Előbb mentünk, és csak siker esetén cseréljük a futó konfigot: így a vezérlő, a kijelző és a web
  // sosem látnak olyan értéket, ami nem került az NVS-be.
  if (!save(c)) { snprintf(err, n, "NVS iras sikertelen"); return false; }
  cfg_ = c;
  loadedOk_ = true;
  version_ = version_ + 1;
  return true;
}
