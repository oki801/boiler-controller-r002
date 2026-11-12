#pragma once
#include <stdint.h>
#include <string.h>

// WiFi-hálózat kipróbálása MENTÉS NÉLKÜL: a beállítás csak sikeres csatlakozás után kerül a konfigba, így egy
// elírt jelszó nem veszi el a működő kapcsolatot. Tiszta állapotgép (hardver nélkül tesztelhető); a tényleges
// WiFi-hívásokat a WifiManager végzi az update() által kért műveletek szerint.
//   Idle -> Pending (kérés; rövid késleltetés, hogy a HTTP-válasz még kiérjen) -> Trying -> Connected -> Saved
//                                                                               \-> Failed (időtúllépés)
class WifiTrial {
 public:
  enum State : uint8_t { Idle, Pending, Trying, Connected, Saved, Failed };
  enum Action : uint8_t { None, Start, Restore };            // Start: csatlakozás indítása; Restore: vissza a mentett hálózatra
  enum Reason : uint8_t { NoReason, NoSsid, BadPassword, Timeout, SaveFailed };

  static constexpr uint32_t kDelayMs = 500;                  // a kérés és a tényleges váltás között
  static constexpr uint32_t kTimeoutMs = 20000;
  static constexpr int kWlNoSsidAvail = 1, kWlConnectFailed = 4, kWlConnectionLost = 5;   // az Arduino WiFi állapotkódjai

  // false: érvénytelen adat (SSID 1..32, jelszó üres vagy 8..64 karakter), vagy már fut egy kipróbálás
  bool request(const char* ssid, const char* pass, uint32_t now) {
    if (state_ == Pending || state_ == Trying) return false;
    const size_t sl = ssid ? strlen(ssid) : 0, pl = pass ? strlen(pass) : 0;
    if (sl < 1 || sl > 32) return false;
    if (pl != 0 && (pl < 8 || pl > 64)) return false;
    memcpy(ssid_, ssid, sl + 1);
    if (pl) memcpy(pass_, pass, pl + 1); else pass_[0] = 0;
    state_ = Pending; reason_ = NoReason; reqMs_ = now;
    seenNoSsid_ = seenFail_ = false;
    return true;
  }

  // Ciklusonként hívandó. wlStatus: az Arduino WiFi.status() értéke.
  Action update(uint32_t now, bool connected, int wlStatus) {
    if (state_ == Pending) {
      if ((uint32_t)(now - reqMs_) < kDelayMs) return None;
      state_ = Trying; startMs_ = now;
      return Start;
    }
    if (state_ != Trying) return None;
    if (connected) { state_ = Connected; return None; }
    if (wlStatus == kWlNoSsidAvail) seenNoSsid_ = true;
    if (wlStatus == kWlConnectFailed || wlStatus == kWlConnectionLost) seenFail_ = true;
    if ((uint32_t)(now - startMs_) >= kTimeoutMs) {
      state_ = Failed;
      reason_ = seenNoSsid_ ? NoSsid : (seenFail_ ? BadPassword : Timeout);
      return Restore;
    }
    return None;
  }

  // A mentés eredménye (Connected után hívandó)
  void saved(bool ok) {
    if (state_ != Connected) return;
    if (ok) state_ = Saved; else { state_ = Failed; reason_ = SaveFailed; }
  }

  State state() const { return state_; }
  Reason reason() const { return reason_; }
  const char* ssid() const { return ssid_; }
  const char* pass() const { return pass_; }

 private:
  State state_ = Idle;
  Reason reason_ = NoReason;
  char ssid_[33] = "", pass_[65] = "";
  uint32_t reqMs_ = 0, startMs_ = 0;
  bool seenNoSsid_ = false, seenFail_ = false;
};
