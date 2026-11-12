#include "WebService.h"
#include "SnapshotJson.h"
#include "OtaService.h"
#include <LittleFS.h>
#include <Update.h>
#include <Sha256Hex.h>
#include <mbedtls/base64.h>
#include <config_defaults.h>
#include <version.h>
#include <ui_html.gz.h>
#include <ui_dash.gz.h>
#include <ui_dash2.gz.h>
#include <ui_dash3.gz.h>
#include <ui_service.gz.h>

bool WebService::authorized(AsyncWebServerRequest* r) {
  if (!r->hasHeader("Authorization")) return false;
  const String h = r->header("Authorization");
  if (!h.startsWith("Basic ")) return false;
  unsigned char out[96];
  size_t olen = 0;
  const String b64 = h.substring(6);
  if (mbedtls_base64_decode(out, sizeof(out) - 1, &olen, (const unsigned char*)b64.c_str(), b64.length()) != 0) return false;
  out[olen] = 0;
  char* colon = strchr((char*)out, ':');
  if (!colon) return false;
  *colon = 0;
  const char* user = (const char*)out;
  const char* pass = colon + 1;
  const Config c = cfg_.get();
  char hash[65];
  sha256Hex(pass, hash);
  const char* want = c.webPassHash[0] ? c.webPassHash : nullptr;
  char defHash[65];
  if (!want) { sha256Hex(DEFAULT_WEB_PASSWORD, defHash); want = defHash; }
  unsigned diff = strcmp(user, c.webUser) != 0;           // állandó idejű összehasonlítás a hash-re
  for (int i = 0; i < 64; ++i) diff |= (unsigned)(hash[i] ^ want[i]);
  return diff == 0;
}

void WebService::deny(AsyncWebServerRequest* r) {
  AsyncWebServerResponse* resp = r->beginResponse(401, "text/plain", "Hitelesites szukseges");
  resp->addHeader("WWW-Authenticate", "Basic realm=\"Bojler\"");
  r->send(resp);
}

void WebService::sendJson(AsyncWebServerRequest* r, JsonDocument& d, int code) {
  String s;
  serializeJson(d, s);
  AsyncWebServerResponse* resp = r->beginResponse(code, "application/json", s);
  resp->addHeader("Cache-Control", "no-store");
  r->send(resp);
}

void WebService::sendErr(AsyncWebServerRequest* r, const char* msg, int code) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = msg;
  sendJson(r, d, code);
}

#define REQUIRE_AUTH(r) if (!authorized(r)) { deny(r); return; }

void WebService::begin() {
  // ---- állapot ----
  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    const Snapshot s = snap_.snapshot();
    JsonDocument d;
    snapjson::state(d["state"].to<JsonObject>(), s);
    snapjson::telemetry(d["tele"].to<JsonObject>(), s);
    snapjson::service(d["svc"].to<JsonObject>(), s);
    JsonObject sys = d["sys"].to<JsonObject>();
    sys["fw"] = s.fw; sys["wifi"] = s.wifiConnected; sys["ap"] = s.apMode;
    sys["mqtt"] = s.mqttConnected; sys["default_creds"] = s.defaultCreds;
    sys["notify_on"] = s.notifyOn; sys["notify_status"] = notify_.status();
    sys["fs_used"] = LittleFS.usedBytes(); sys["fs_total"] = LittleFS.totalBytes();
    JsonArray names = d["names"].to<JsonArray>();
    const Config c = cfg_.get();
    for (uint8_t i = 0; i < kSources; ++i) names.add(c.srcName[i]);
    sendJson(r, d);
  });

  // ---- konfiguráció ----
  server_.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    JsonDocument d;
    cfg_.toJson(d, false);                            // jelszavak nélkül
    sendJson(r, d);
  });
  auto* cfgH = new AsyncCallbackJsonWebHandler("/api/config", [this](AsyncWebServerRequest* r, JsonVariant& json) {
    REQUIRE_AUTH(r);
    char err[64] = "";
    JsonDocument d;
    if (cfg_.patch(json, err, sizeof(err))) { d["ok"] = true; sendJson(r, d); }
    else sendErr(r, err);
  });
  cfgH->setMethod(HTTP_POST);
  server_.addHandler(cfgH);

  // ---- vezérlés (csak kérések) ----
  auto* ctlH = new AsyncCallbackJsonWebHandler("/api/control", [this](AsyncWebServerRequest* r, JsonVariant& json) {
    REQUIRE_AUTH(r);
    JsonDocument d;
    const char* m = json["mode"] | "";
    if (!strcmp(m, "auto")) sink_.setMode(Mode::Auto, kNone);
    else if (!strcmp(m, "off")) sink_.setMode(Mode::Off, kNone);
    else if (!strcmp(m, "force")) {
      const int f = json["force"] | 0;
      if (f < 1 || f > kSources) { sendErr(r, "force: 1..3"); return; }
      sink_.setMode(Mode::Force, (int8_t)(f - 1));
    } else if (m[0]) { sendErr(r, "mode: auto|force|off"); return; }
    if (json["ext_ok"].is<JsonObject>()) {
      const int s = json["ext_ok"]["src"] | 0;
      if (s >= 1 && s <= kSources) sink_.setExtOk((uint8_t)(s - 1), json["ext_ok"]["value"] | false);
    }
    d["ok"] = true;
    sendJson(r, d);
  });
  ctlH->setMethod(HTTP_POST);
  server_.addHandler(ctlH);

  server_.on("/api/fault/ack", HTTP_POST, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    sink_.ackFault();
    JsonDocument d; d["ok"] = true; sendJson(r, d);
  });
  server_.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    JsonDocument d; d["ok"] = true; sendJson(r, d);
    restartAt_ = millis() + 800;
  });

  // ---- szervizmód (szárazfutás): belépés/kilépés újraindítással, szimulált bemenetek ----
  // Szervizmódban a valódi kontaktorok NEM mozdulnak, a vezérlés szimulált kontaktorokkal és szimulált
  // bemenetekkel fut. Az /api/service/sim csak aktív szervizmódban működik (különben 403).
  auto* svcEnterH = new AsyncCallbackJsonWebHandler("/api/service/enter", [this](AsyncWebServerRequest* r, JsonVariant& json) {
    REQUIRE_AUTH(r);
    if (!(json["confirm"] | false)) { sendErr(r, "megerosites kell (confirm: true)"); return; }
    const int m = json["minutes"] | (int)kSvcDefaultMinutes;
    if (m < (int)kSvcMinMinutes || m > (int)kSvcMaxMinutes) { sendErr(r, "minutes: 1..30"); return; }
    if (snap_.snapshot().svc) { sendErr(r, "a szervizmod mar aktiv", 409); return; }
    if (!sink_.serviceRequest(true, (uint16_t)m)) { sendErr(r, "a szervizmod nem indithato", 500); return; }
    JsonDocument d; d["ok"] = true; d["minutes"] = m; sendJson(r, d);
    restartAt_ = millis() + 800;                      // a válasz kimegy, utána újraindul (a kontaktorok elengednek)
  });
  svcEnterH->setMethod(HTTP_POST);
  server_.addHandler(svcEnterH);

  server_.on("/api/service/exit", HTTP_POST, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    if (!snap_.snapshot().svc) { sendErr(r, "a szervizmod nem aktiv", 409); return; }
    if (!sink_.serviceRequest(false, 0)) { sendErr(r, "a kilepes nem sikerult", 500); return; }
    JsonDocument d; d["ok"] = true; sendJson(r, d);
    restartAt_ = millis() + 800;
  });

  auto* svcSimH = new AsyncCallbackJsonWebHandler("/api/service/sim", [this](AsyncWebServerRequest* r, JsonVariant& json) {
    REQUIRE_AUTH(r);
    ServiceSim s;
    const char* err = "";
    if (!snapjson::parseSim(json, s, err)) { sendErr(r, err); return; }
    if (!sink_.serviceSim(s)) { sendErr(r, "a szervizmod nem aktiv", 403); return; }
    JsonDocument d; d["ok"] = true; sendJson(r, d);
  });
  svcSimH->setMethod(HTTP_POST);
  server_.addHandler(svcSimH);

  // ---- 24 órás előzmény (PSRAM) ----
  server_.on("/api/history", HTTP_GET, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    const uint16_t pts = r->hasParam("points") ? (uint16_t)r->getParam("points")->value().toInt() : 288;
    AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", hist_.json(pts ? pts : 288));
    resp->addHeader("Cache-Control", "no-store");
    r->send(resp);
  });

  // ---- vízkő-/fűtőelem-trend ----
  server_.on("/api/trend", HTTP_GET, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", trend_.json());
    resp->addHeader("Cache-Control", "no-store");
    r->send(resp);
  });
  server_.on("/api/trend/reset", HTTP_POST, [this](AsyncWebServerRequest* r) {     // vízkőmentesítés után
    REQUIRE_AUTH(r);
    trend_.reset();
    JsonDocument d; d["ok"] = true; sendJson(r, d);
  });

  // ---- értesítés: teszt üzenet ----
  server_.on("/api/notify/test", HTTP_POST, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    notify_.sendTest();
    JsonDocument d; d["ok"] = true; sendJson(r, d);
  });

  // ---- naplók ----
  server_.on("/api/events", HTTP_GET, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    r->send(200, "text/csv; charset=utf-8", log_.eventsTail(6000));
  });
  server_.on("/api/log/files", HTTP_GET, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    r->send(200, "application/json", log_.listFiles());
  });
  server_.on("/api/log/download", HTTP_GET, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    if (!r->hasParam("f")) { sendErr(r, "f hianyzik"); return; }
    const String f = r->getParam("f")->value();
    if (!LogService::safeName(f)) { sendErr(r, "ervenytelen fajlnev"); return; }
    const String path = "/log/" + f;
    if (!LittleFS.exists(path)) { sendErr(r, "nincs ilyen fajl", 404); return; }
    r->send(LittleFS, path, "text/csv", true);
  });

  // ---- WiFi: hálózatkeresés, kipróbálás mentés nélkül, állapot (a beállítások oldal "WiFi" paneljéhez) ----
  // GET /api/wifi/scan: elindítja a keresést; {"state":"running"} amíg fut, majd {"state":"done","nets":[...]}
  server_.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    JsonDocument d;
    const int n = wifi_.scanResult();
    if (n < 0) {
      if (n == -2) wifi_.scanStart();                     // még nem indult el: indítjuk (a -1 már fut)
      d["state"] = "running";
      sendJson(r, d);
      return;
    }
    int idx[24]; int cnt = 0;
    for (int i = 0; i < n && cnt < 24; ++i) {
      const String s = WiFi.SSID(i);
      if (s.length() == 0) continue;                      // rejtett hálózat: kézzel adható meg
      int dup = -1;
      for (int k = 0; k < cnt; ++k) if (WiFi.SSID(idx[k]) == s) { dup = k; break; }
      if (dup >= 0) { if (WiFi.RSSI(i) > WiFi.RSSI(idx[dup])) idx[dup] = i; continue; }   // azonos nevű: a legerősebb marad
      idx[cnt++] = i;
    }
    for (int a = 1; a < cnt; ++a) {                       // jelerősség szerint csökkenő sorrend
      const int v = idx[a]; int b = a - 1;
      while (b >= 0 && WiFi.RSSI(idx[b]) < WiFi.RSSI(v)) { idx[b + 1] = idx[b]; --b; }
      idx[b + 1] = v;
    }
    d["state"] = "done";
    JsonArray nets = d["nets"].to<JsonArray>();
    for (int k = 0; k < cnt; ++k) {
      JsonObject o = nets.add<JsonObject>();
      o["ssid"] = String(WiFi.SSID(idx[k]));
      o["rssi"] = WiFi.RSSI(idx[k]);
      o["secure"] = WiFi.encryptionType(idx[k]) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    sendJson(r, d);
  });

  // POST /api/wifi/connect {"ssid":"...","pass":"..."}: kipróbálja a hálózatot; csak sikeres csatlakozás után ment
  auto* wifiConnH = new AsyncCallbackJsonWebHandler("/api/wifi/connect", [this](AsyncWebServerRequest* r, JsonVariant& json) {
    REQUIRE_AUTH(r);
    const char* ssid = json["ssid"] | "";
    const char* pass = json["pass"] | "";
    const WifiTrial::State st = wifi_.trial().state();
    if (st == WifiTrial::Pending || st == WifiTrial::Trying) { sendErr(r, "mar fut egy csatlakozas", 409); return; }
    const size_t sl = strlen(ssid), pl = strlen(pass);
    if (sl < 1 || sl > 32) { sendErr(r, "a halozat neve 1-32 karakter"); return; }
    if (pl != 0 && (pl < 8 || pl > 64)) { sendErr(r, "a jelszo 8-64 karakter (nyitott halozatnal ures)"); return; }
    if (!wifi_.trialRequest(ssid, pass)) { sendErr(r, "a kerest nem lehetett elinditani"); return; }
    JsonDocument d; d["ok"] = true; sendJson(r, d);
  });
  wifiConnH->setMethod(HTTP_POST);
  server_.addHandler(wifiConnH);

  // GET /api/wifi/status: a kipróbálás állapota, az aktuális hálózat és az IP-cím
  server_.on("/api/wifi/status", HTTP_GET, [this](AsyncWebServerRequest* r) {
    REQUIRE_AUTH(r);
    const WifiTrial& t = wifi_.trial();
    JsonDocument d;
    switch (t.state()) {
      case WifiTrial::Idle:      d["state"] = "idle"; break;
      case WifiTrial::Saved:     d["state"] = "ok"; break;
      case WifiTrial::Failed:    d["state"] = "failed"; break;
      default:                   d["state"] = "trying"; break;       // Pending, Trying, Connected (mentés folyamatban)
    }
    if (t.state() != WifiTrial::Idle) d["trial_ssid"] = String(t.ssid());
    if (t.state() == WifiTrial::Failed) {
      switch (t.reason()) {
        case WifiTrial::NoSsid:       d["reason"] = "a halozat nem talalhato"; break;
        case WifiTrial::BadPassword:  d["reason"] = "a csatlakozas sikertelen (hibas jelszo?)"; break;
        case WifiTrial::SaveFailed:   d["reason"] = "a halozat mukodik, de a mentes nem sikerult"; break;
        default:                      d["reason"] = "idotullepes"; break;
      }
    }
    d["connected"] = wifi_.connected();
    d["ssid"] = wifi_.ssid();
    d["ip"] = wifi_.staIp();
    d["rssi"] = wifi_.rssi();
    d["ap"] = wifi_.apMode();
    d["ap_ip"] = wifi_.apIp();
    d["host"] = String(cfg_.get().deviceId) + ".local";
    sendJson(r, d);
  });

  // ---- OTA (firmware vagy fájlrendszer): POST /update[?type=fs] ----
  server_.on("/update", HTTP_POST,
    [this](AsyncWebServerRequest* r) {
      if (!authorized(r)) { deny(r); return; }
      const bool ok = !Update.hasError();
      OtaService::setBusy(false);
      AsyncWebServerResponse* resp = r->beginResponse(ok ? 200 : 500, "text/plain", ok ? "OK, ujraindul" : "Frissites sikertelen");
      resp->addHeader("Connection", "close");
      r->send(resp);
      if (ok) restartAt_ = millis() + 1200;
    },
    [this](AsyncWebServerRequest* r, String, size_t index, uint8_t* data, size_t len, bool final) {
      if (!authorized(r)) return;
      if (index == 0) {
        const bool fs = r->hasParam("type") && r->getParam("type")->value() == "fs";
        OtaService::setBusy(true);                        // frissítés alatt a váltás tiltva
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, fs ? U_SPIFFS : U_FLASH)) OtaService::setBusy(false);
      }
      if (Update.isRunning()) Update.write(data, len);
      if (final && Update.isRunning()) Update.end(true);
    });

  // ---- felület ----
  // A felület a firmware-be van ágyazva (gzip), így nem függ a LittleFS-től. A héj nem tartalmaz adatot.
  auto serveUi = [](AsyncWebServerRequest* r) {
    AsyncWebServerResponse* resp = r->beginResponse_P(200, "text/html; charset=utf-8", UI_HTML_GZ, UI_HTML_GZ_LEN);
    resp->addHeader("Content-Encoding", "gzip");
    resp->addHeader("Cache-Control", "max-age=300");
    r->send(resp);
  };
  server_.on("/", HTTP_GET, serveUi);
  server_.on("/index.html", HTTP_GET, serveUi);
  // Felhasznaloi dashboardok (kulon HTML-ek, a heju nem tartalmaz adatot; az adatok az API-n at, hitelesitve jonnek)
  //   /dash  : az eredeti, valtozatlan, statikus bemutato
  //   /dash2 : az adatkotott alverzio (celhomerseklet, allapot, halozatok, naplo, hibak)
//   /dash3 : ugyanaz folyekony mozgassal (viz a tarcsaban, athuzo ertekek, csuszo menu)
  auto serveGz = [](const uint8_t* data, size_t len) {
    return [data, len](AsyncWebServerRequest* r) {
      AsyncWebServerResponse* resp = r->beginResponse_P(200, "text/html; charset=utf-8", data, len);
      resp->addHeader("Content-Encoding", "gzip");
      resp->addHeader("Cache-Control", "max-age=300");
      r->send(resp);
    };
  };
  server_.on("/dash", HTTP_GET, serveGz(UI_DASH_GZ, UI_DASH_GZ_LEN));
  server_.on("/dash2", HTTP_GET, serveGz(UI_DASH2_GZ, UI_DASH2_GZ_LEN));
  server_.on("/dash3", HTTP_GET, serveGz(UI_DASH3_GZ, UI_DASH3_GZ_LEN));   // v3: folyékony mozgás, a kijelzővel közös design-nyelv
  server_.on("/service", HTTP_GET, serveGz(UI_SERVICE_GZ, UI_SERVICE_GZ_LEN));   // szervizoldal (a HTML-héj nem tartalmaz adatot, az API hitelesít)
  server_.onNotFound([](AsyncWebServerRequest* r) { r->send(404, "text/plain", "Nem talalhato"); });
  server_.begin();
}

void WebService::loop() {
  if (restartAt_ && (int32_t)(millis() - restartAt_) >= 0) ESP.restart();   // újraindításkor a kontaktorok elengednek
}
