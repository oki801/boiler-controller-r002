#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <EventBus.h>
#include <NotifyPolicy.h>
#include <ConfigStore.h>
#include <TimeService.h>
#include <Sync.h>
#include <freertos/queue.h>

// Értesítések kritikus hibáról. ALAPBÓL KI (notify.kind = 0), a webes felületen kapcsolható be.
// Csak kifelé kezdeményez kapcsolatot, az eszköz nem fogad az internetről. Csatornák:
//   1 = Telegram bot, 2 = webhook (JSON), 3 = ntfy (saját vagy nyilvános szerver)
// A helyi értesítés (kijelző, LED, MQTT /error topic, Home Assistant) ettől függetlenül mindig működik.
// FIGYELEM: a HTTPS jelenleg nem ellenőrzi a szerver tanúsítványát (setInsecure). Éles internetes használat
// előtt tanúsítvány-ellenőrzés vagy helyi továbbító (pl. Home Assistant / saját ntfy) javasolt.
class NotifyService {
 public:
  NotifyService(ConfigStore& cfg, const TimeService& t) : cfg_(cfg), time_(t) {}
  void begin();
  static void eventThunk(const Event& e, void* ctx);    // EventBus-feliratkozó (szálbiztos, nem blokkol)
  void sendTest();                                       // teszt üzenet (a háttér-task küldi)
  String status() const;                                 // utolsó küldés eredménye
 private:
  struct Msg { char title[72]; char text[220]; uint8_t sev; };
  static void task(void* p);
  void loop();
  void deliver(const Config& c, const Msg& m);
  bool send(const Config& c, const Msg& m, String& why);
  bool post(const char* url, const char* ctype, const String& body, const char* bearer,
            const char* title, const char* prio, String& why);
  void setStatus(const char* s);

  ConfigStore& cfg_;
  const TimeService& time_;
  QueueHandle_t q_ = nullptr;
  NotifyPolicy policy_;
  mutable Mutex mtx_;
  char status_[96] = "nincs kuldve";
};
