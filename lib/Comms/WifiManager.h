#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Config.h>
#include <WifiTrial.h>

// STA-kapcsolat; ha 30 s alatt nem sikerül (vagy nincs SSID), saját AP indul (Bojler-XXXX),
// közben a STA-újracsatlakozás folytatódik.
class WifiManager {
 public:
  void begin(const Config& c);
  void configure(const Config& c);       // futás közbeni WiFi-adatcsere
  void loop();
  bool connected() const { return WiFi.status() == WL_CONNECTED; }
  bool apMode() const { return ap_; }
  int8_t rssi() const { return connected() ? (int8_t)WiFi.RSSI() : 0; }
  uint32_t ip32() const { return connected() ? (uint32_t)WiFi.localIP() : (ap_ ? (uint32_t)WiFi.softAPIP() : 0u); }   // 0: nincs cím
  // Hálózatkeresés és kipróbálás: a beállítás csak sikeres csatlakozás után mentődik (lásd WifiTrial)
  void scanStart();                                    // aszinkron keresés indítása
  int  scanResult() const { return WiFi.scanComplete(); }   // -2: nincs, -1: fut, >=0: találatok száma
  bool trialRequest(const char* ssid, const char* pass) { return trial_.request(ssid, pass, millis()); }
  const WifiTrial& trial() const { return trial_; }
  void trialSaved(bool ok);                            // a Supervisor hívja a konfig mentése után
  String ssid() const { return connected() ? WiFi.SSID() : String(""); }
  String staIp() const { return connected() ? WiFi.localIP().toString() : String(""); }
  String apIp() const { return ap_ ? WiFi.softAPIP().toString() : String(""); }
  String ip() const { return connected() ? WiFi.localIP().toString() : (ap_ ? WiFi.softAPIP().toString() : String("")); }
 private:
  void startAp();
  void restore();                                      // sikertelen kipróbálás után vissza a mentett hálózatra
  void stopAp();
  char host_[24] = "boiler01";
  char curSsid_[33] = "", curPass_[65] = "";            // az éppen mentett hálózat (a visszaállításhoz)
  WifiTrial trial_;
  bool hasSsid_ = false, ap_ = false, mdns_ = false, wasConnected_ = false;
  uint32_t startMs_ = 0, connectedSinceMs_ = 0;
};
