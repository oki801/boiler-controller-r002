#include "WifiManager.h"
#include <config_defaults.h>

void WifiManager::begin(const Config& c) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  configure(c);
}

void WifiManager::configure(const Config& c) {
  strlcpy(host_, c.deviceId, sizeof(host_));
  hasSsid_ = c.wifiSsid[0] != 0;
  WiFi.disconnect(false);
  WiFi.setHostname(host_);
  startMs_ = millis();
  if (hasSsid_) {
    WiFi.mode(ap_ ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(c.wifiSsid, c.wifiPass);
  } else {
    startAp();
  }
}

void WifiManager::startAp() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char ssid[24];
  snprintf(ssid, sizeof(ssid), "Bojler-%02X%02X", mac[4], mac[5]);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, AP_PASSWORD);
  ap_ = true;
}

void WifiManager::stopAp() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  ap_ = false;
}

void WifiManager::loop() {
  const uint32_t now = millis();
  const bool up = connected();
  if (up && !wasConnected_) {
    connectedSinceMs_ = now;
    if (!mdns_ && MDNS.begin(host_)) { MDNS.addService("http", "tcp", 80); mdns_ = true; }
  }
  wasConnected_ = up;
  if (up && ap_ && (uint32_t)(now - connectedSinceMs_) > 10000) stopAp();     // elég stabil a STA
  if (!up && !ap_ && hasSsid_ && (uint32_t)(now - startMs_) > 30000) startAp();  // tartalék AP
}
