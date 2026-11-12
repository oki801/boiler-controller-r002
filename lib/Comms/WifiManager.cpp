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
  strlcpy(curSsid_, c.wifiSsid, sizeof(curSsid_));
  strlcpy(curPass_, c.wifiPass, sizeof(curPass_));
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

void WifiManager::scanStart() {
  if (trial_.state() == WifiTrial::Pending || trial_.state() == WifiTrial::Trying) return;   // kipróbálás közben nem keresünk
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;
  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);                       // aszinkron, a rejtett hálózatokat is
}

void WifiManager::restore() {
  WiFi.disconnect(false);
  startMs_ = millis();
  if (curSsid_[0]) { WiFi.mode(ap_ ? WIFI_AP_STA : WIFI_STA); WiFi.begin(curSsid_, curPass_); }
  else startAp();
}

void WifiManager::trialSaved(bool ok) {
  if (ok) {
    strlcpy(curSsid_, trial_.ssid(), sizeof(curSsid_));
    strlcpy(curPass_, trial_.pass(), sizeof(curPass_));
    hasSsid_ = true;
  }
  trial_.saved(ok);
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
  const WifiTrial::Action act = trial_.update(now, up, (int)WiFi.status());
  if (act == WifiTrial::Start) {                       // a kérés HTTP-válasza már kiment: most váltunk
    WiFi.scanDelete();
    WiFi.disconnect(false);
    WiFi.mode(ap_ ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(trial_.ssid(), trial_.pass());
    startMs_ = now;
  } else if (act == WifiTrial::Restore) {
    restore();
  }
  const bool fresh = trial_.state() == WifiTrial::Connected || trial_.state() == WifiTrial::Saved;
  // a saját AP a sikeres csatlakozás után még 60 s-ig él, hogy a felhasználó lássa az új IP-címet
  if (up && ap_ && (uint32_t)(now - connectedSinceMs_) > (fresh ? 60000UL : 10000UL)) stopAp();
  if (!up && !ap_ && hasSsid_ && (uint32_t)(now - startMs_) > 30000) startAp();  // tartalék AP
}
