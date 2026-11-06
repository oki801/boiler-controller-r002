#pragma once
// Eszköz-konfiguráció (tiszta POD + validáció, hardver nélkül tesztelhető).
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "BoilerTypes.h"

struct Config {
  uint16_t  ver = 1;
  char      deviceId[24] = "boiler01";
  bool      preempt = true;            // magasabb prioritású forrás átvehet (min. tartózkodás után)
  uint16_t  logIntervalS = 60;
  uint8_t   logRetentionDays = 14;
  SourceCfg src[kSources];
  char      srcName[kSources][20];
  GuaranteeCfg guar;
  float     capHystC = 3.0f;           // hőfokhatár-hiszterézis
  float     targetC = 55.0f;           // célhőmérséklet; 0 = kikapcsolva (nem fűt)
  float     hystC = 2.0f;              // termosztát-hiszterézis
  TimingCfg timing;
  SafetyCfg safety;

  char      wifiSsid[33] = "";
  char      wifiPass[65] = "";

  bool      mqttEnabled = false;
  char      mqttHost[64] = "";
  uint16_t  mqttPort = 1883;
  char      mqttUser[32] = "";
  char      mqttPass[64] = "";
  char      mqttBase[48] = "";         // üres: boiler/<deviceId>

  char      webUser[16] = "admin";
  char      webPassHash[65] = "";      // SHA-256 hex; üres: alapértelmezett jelszó (config_defaults.h)
  char      otaPass[32] = "";

  // Értesítés (alapból KI). 0 = ki, 1 = Telegram, 2 = webhook (JSON), 3 = ntfy
  uint8_t   notifyKind = 0;
  uint8_t   notifyMinSev = 3;          // Severity: 0 info, 1 warn, 2 error, 3 kritikus
  char      notifyUrl[128] = "";
  char      notifyToken[72] = "";      // Telegram bot token vagy ntfy bearer token
  char      notifyChat[24] = "";       // Telegram chat azonosító

  // Vízkő-/fűtőelem-trendfigyelés
  bool      trendEnabled = true;
  float     trendThresholdPct = 10.0f;

  Config() {
    for (uint8_t i = 0; i < kSources; ++i) {
      src[i].priority = i + 1;
      snprintf(srcName[i], sizeof(srcName[i]), "Halozat %d", i + 1);
    }
  }
};

inline bool validateConfig(const Config& c, char* err, size_t n) {
  auto bad = [&](const char* m) { if (err && n) snprintf(err, n, "%s", m); return false; };
  const TimingCfg& t = c.timing;
  if (t.deadMs < kHardMinDeadMs || t.deadMs > 60000)         return bad("timing.deadMs: 2000..60000");
  if (t.closeTimeoutMs < 200 || t.closeTimeoutMs > 5000)     return bad("timing.closeMs: 200..5000");
  if (t.openTimeoutMs  < 200 || t.openTimeoutMs  > 5000)     return bad("timing.openMs: 200..5000");
  if (t.minDwellMs > 3600000UL)                              return bad("timing.dwellMs: max 3600000");
  if (t.availOnMs > 600000UL)                                return bad("timing.availOnMs: max 600000");
  if (t.availOffMs < 200 || t.availOffMs > 60000)            return bad("timing.availOffMs: 200..60000");
  if (t.retryMs < 5000)                                      return bad("timing.retryMs: min 5000");
  const SafetyCfg& s = c.safety;
  if (s.tMaxC < 40.0f || s.tMaxC > 95.0f)                    return bad("safety.tMaxC: 40..95");
  if (s.tHystC < 1.0f || s.tHystC > 20.0f)                   return bad("safety.tHystC: 1..20");
  if (!(s.vMin < s.vMax) || s.vMin < 100.0f || s.vMax > 300.0f) return bad("safety.vMin/vMax");
  if (s.maxSwitchesPerHour < 1 || s.maxSwitchesPerHour > 30) return bad("safety.maxSwitchesPerHour: 1..30");
  if (s.maxFailures < 1 || s.maxFailures > 10)               return bad("safety.maxFailures: 1..10");
  if (c.logIntervalS < 10 || c.logIntervalS > 600)           return bad("logIntervalS: 10..600");
  if (c.logRetentionDays < 1 || c.logRetentionDays > 90)     return bad("logRetentionDays: 1..90");
  for (uint8_t i = 0; i < kSources; ++i) {
    const SourceCfg& sc = c.src[i];
    if (sc.priority < 1 || sc.priority > kSources)           return bad("src.priority: 1..3");
    if (sc.schedFrom > 1439 || sc.schedTo > 1439)            return bad("src.from/to: 0..1439");
    if ((sc.dayMask & 0x7F) == 0)                            return bad("src.days: legalabb egy nap");
    if (sc.maxTempC < 30.0f || sc.maxTempC > 90.0f)          return bad("src.maxTemp: 30..90");
    if (sc.maxTempC > c.safety.tMaxC)                        return bad("src.maxTemp nem lehet nagyobb a vedelmi hatarnal");
  }
  if (c.targetC != 0.0f && (c.targetC < 10.0f || c.targetC > c.safety.tMaxC)) return bad("target: 0 (ki) vagy 10..vedelmi hatar");
  if (c.hystC < 0.5f || c.hystC > 10.0f)                     return bad("hyst: 0.5..10");
  if (c.capHystC < 1.0f || c.capHystC > 15.0f)               return bad("capHystC: 1..15");
  const GuaranteeCfg& g = c.guar;
  if (g.readyAtMin > 1439 || g.untilMin > 1439)              return bad("guarantee ido: 0..1439");
  if (g.leadMin > 360)                                       return bad("guarantee.lead: max 360");
  if (g.minTempC < 30.0f || g.minTempC > 70.0f)              return bad("guarantee.minTemp: 30..70");
  if (g.hystC < 1.0f || g.hystC > 10.0f)                     return bad("guarantee.hyst: 1..10");
  if (g.enabled) {                                           // legyen olyan forrás, ami el is éri a minimumot
    bool ok = false;
    for (uint8_t i = 0; i < kSources; ++i)
      if (c.src[i].guarantee && c.src[i].enabled && c.src[i].maxTempC >= g.minTempC + g.hystC) ok = true;
    if (!ok) return bad("guarantee: nincs forras, aminek a hofokhatara eleri a minimumot");
  }
  if (c.notifyKind > 3 || c.notifyMinSev > 3)                return bad("notify.kind/minSev");
  if (c.notifyKind == 1 && (c.notifyToken[0] == 0 || c.notifyChat[0] == 0)) return bad("telegram: token es chat kell");
  if ((c.notifyKind == 2 || c.notifyKind == 3) &&
      strncmp(c.notifyUrl, "http://", 7) != 0 && strncmp(c.notifyUrl, "https://", 8) != 0) return bad("notify.url: http(s)://");
  if (c.trendThresholdPct < 3.0f || c.trendThresholdPct > 40.0f) return bad("trend.thresholdPct: 3..40");
  if (c.deviceId[0] == 0)                                    return bad("deviceId ures");
  for (const char* p = c.deviceId; *p; ++p) {
    const char ch = *p;
    const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
    if (!ok) return bad("deviceId: csak a-z 0-9 - _");
  }
  if (c.mqttEnabled && c.mqttHost[0] == 0)                   return bad("mqtt.host kell");
  if (c.webUser[0] == 0)                                     return bad("web.user ures");
  return true;
}
