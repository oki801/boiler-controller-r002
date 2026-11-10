#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Config.h>

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
  String ip() const { return connected() ? WiFi.localIP().toString() : (ap_ ? WiFi.softAPIP().toString() : String("")); }
 private:
  void startAp();
  void stopAp();
  char host_[24] = "boiler01";
  bool hasSsid_ = false, ap_ = false, mdns_ = false, wasConnected_ = false;
  uint32_t startMs_ = 0, connectedSinceMs_ = 0;
};
