#pragma once
// Alapértelmezett értékek, amelyek nem tartoznak a Config struktúrába.
#define TZ_BUDAPEST          "CET-1CEST,M3.5.0,M10.5.0/3"
#define DEFAULT_WEB_PASSWORD "boiler"      // első indításkor; ELSŐ LÉPÉSKÉNT CSERÉLD LE a webes felületen!
#define DEFAULT_OTA_PASSWORD "boiler-ota"  // ha a konfigurációban nincs OTA jelszó
#define AP_PASSWORD          "bojler-setup"
constexpr unsigned long kControlPeriodMs = 100;
constexpr unsigned long kEnergyPersistMs = 600000UL;   // 10 perc
