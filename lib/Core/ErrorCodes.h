#pragma once
#include <stdint.h>

enum class ErrorCode : uint8_t {
  None = 0,
  E01 = 1,  // DS18B20 hiba
  E02,      // PZEM kommunikációs hiba
  E03,      // reteszelési sértés
  E04,      // kontaktor nem húz be
  E05,      // kontaktor nem enged el
  E06,      // feszültség tartományon kívül
  E07,      // túlhőmérséklet
  E08,      // WiFi / MQTT kiesés
  E09,      // nincs érvényes idő
  E10,      // gyakori újraindulás
  E11,      // tárhely tele
  E12,      // hibás konfiguráció
  E13,      // kapcsolgatás
  E14,      // nincs jogosult forrás
  E15,      // vízkő/üledék gyanú (effektív hőkapacitás csökken)
  E16,      // fűtőelem teljesítménye csökken
  Count
};

enum class Severity : uint8_t { Info, Warn, Error, Critical };

struct ErrorInfo { const char* code; const char* text; Severity sev; };

inline const ErrorInfo& errorInfo(ErrorCode c) {
  static const ErrorInfo kTable[] = {
    {"--",  "nincs hiba",                    Severity::Info},
    {"E01", "DS18B20 hiba",                  Severity::Warn},
    {"E02", "PZEM kommunikacios hiba",       Severity::Warn},
    {"E03", "Reteszelesi sertes",            Severity::Critical},
    {"E04", "Kontaktor nem huz be",          Severity::Error},
    {"E05", "Kontaktor nem enged el",        Severity::Critical},
    {"E06", "Feszultseg tartomanyon kivul",  Severity::Warn},
    {"E07", "Tuluhomerseklet",               Severity::Error},
    {"E08", "WiFi/MQTT kiesett",              Severity::Info},
    {"E09", "Nincs ervenyes ido (NTP)",      Severity::Warn},
    {"E10", "Gyakori ujrainditas",           Severity::Error},
    {"E11", "Tarhely tele",                  Severity::Warn},
    {"E12", "Hibas konfiguracio",            Severity::Error},
    {"E13", "Tul gyakori kapcsolgatas",      Severity::Warn},
    {"E14", "Nincs jogosult forras",         Severity::Info},
    {"E15", "Vizko/uledek gyanu",            Severity::Warn},
    {"E16", "Futoelem teljesitmeny csokken", Severity::Warn},
  };
  const uint8_t i = static_cast<uint8_t>(c);
  return kTable[i < static_cast<uint8_t>(ErrorCode::Count) ? i : 0];
}
