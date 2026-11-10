#include "NotifyService.h"

namespace { const char* kTest = "notify-test"; }

void NotifyService::begin() {
  q_ = xQueueCreate(16, sizeof(Event));
  xTaskCreatePinnedToCore(task, "notify", 8192, this, 1, nullptr, 0);   // alacsony prioritás: a HTTP-hívás blokkolhat
}

void NotifyService::eventThunk(const Event& e, void* ctx) {
  auto* self = static_cast<NotifyService*>(ctx);
  if (self->q_ && !self->muted_ && e.type == EventType::Error) xQueueSend(self->q_, &e, 0);
}

void NotifyService::sendTest() {
  Event e; e.type = EventType::System; e.ms = millis(); e.setMsg(kTest);
  if (q_) xQueueSend(q_, &e, 0);
}

String NotifyService::status() const { Lock l(mtx_); return String(status_); }

void NotifyService::setStatus(const char* s) { Lock l(mtx_); strlcpy(status_, s, sizeof(status_)); }

void NotifyService::task(void* p) { static_cast<NotifyService*>(p)->loop(); }

void NotifyService::loop() {
  for (;;) {
    Event e;
    if (xQueueReceive(q_, &e, portMAX_DELAY) != pdTRUE) continue;
    const Config c = cfg_.get();
    if (c.notifyKind == 0) continue;                              // kikapcsolva: nem küldünk semmit

    char ts[32]; time_.iso(ts, sizeof(ts));
    Msg m = {};
    if (e.type == EventType::System) {                            // teszt
      snprintf(m.title, sizeof(m.title), "[%s] Teszt", c.deviceId);
      snprintf(m.text, sizeof(m.text), "[%s] Teszt uzenet, az ertesites mukodik. %s", c.deviceId, ts);
      m.sev = 0;
      deliver(c, m);
      continue;
    }
    const NotifyPolicy::Action a = policy_.decide(e.code, e.raised, static_cast<Severity>(c.notifyMinSev), millis());
    if (a == NotifyPolicy::Action::None) continue;
    const ErrorInfo& ei = errorInfo(e.code);
    const char* sevName = ei.sev == Severity::Critical ? "KRITIKUS" : ei.sev == Severity::Error ? "HIBA"
                        : ei.sev == Severity::Warn ? "Figyelmeztetes" : "Info";
    if (a == NotifyPolicy::Action::SendRaised) {
      snprintf(m.title, sizeof(m.title), "[%s] %s %s", c.deviceId, sevName, ei.code);
      snprintf(m.text, sizeof(m.text), "[%s] %s %s: %s (%s)", c.deviceId, sevName, ei.code, e.msg[0] ? e.msg : ei.text, ts);
    } else {
      snprintf(m.title, sizeof(m.title), "[%s] Megszunt %s", c.deviceId, ei.code);
      snprintf(m.text, sizeof(m.text), "[%s] Megszunt: %s %s (%s)", c.deviceId, ei.code, ei.text, ts);
    }
    m.sev = static_cast<uint8_t>(ei.sev);
    deliver(c, m);
  }
}

// Újrapróbálkozás növekvő várakozással; a kapcsolat hiánya nem hiba, megvárjuk.
void NotifyService::deliver(const Config& c, const Msg& m) {
  static const uint32_t waits[] = {15000, 30000, 60000, 120000, 300000};
  String why;
  for (uint8_t attempt = 0; attempt < 6; ++attempt) {
    if (WiFi.status() == WL_CONNECTED && send(c, m, why)) { setStatus("ok"); return; }
    char buf[96];
    snprintf(buf, sizeof(buf), "hiba (%u. proba): %s", attempt + 1, WiFi.status() == WL_CONNECTED ? why.c_str() : "nincs WiFi");
    setStatus(buf);
    if (attempt < 5) vTaskDelay(pdMS_TO_TICKS(waits[attempt]));
  }
  setStatus("elvetve: tobb sikertelen proba utan");
}

bool NotifyService::send(const Config& c, const Msg& m, String& why) {
  if (c.notifyKind == 1) {                                        // Telegram
    JsonDocument d;
    d["chat_id"] = String(c.notifyChat);
    d["text"] = String(m.text);
    String body; serializeJson(d, body);
    const String url = String("https://api.telegram.org/bot") + c.notifyToken + "/sendMessage";
    return post(url.c_str(), "application/json", body, nullptr, nullptr, nullptr, why);
  }
  if (c.notifyKind == 2) {                                        // webhook (JSON)
    JsonDocument d;
    d["device"] = String(c.deviceId);
    d["severity"] = m.sev;
    d["title"] = String(m.title);
    d["text"] = String(m.text);
    String body; serializeJson(d, body);
    return post(c.notifyUrl, "application/json", body, c.notifyToken[0] ? c.notifyToken : nullptr, nullptr, nullptr, why);
  }
  if (c.notifyKind == 3) {                                        // ntfy
    const char* prio = m.sev >= 3 ? "urgent" : m.sev == 2 ? "high" : "default";
    return post(c.notifyUrl, "text/plain; charset=utf-8", String(m.text), c.notifyToken[0] ? c.notifyToken : nullptr,
                m.title, prio, why);
  }
  why = "ismeretlen csatorna";
  return false;
}

bool NotifyService::post(const char* url, const char* ctype, const String& body, const char* bearer,
                         const char* title, const char* prio, String& why) {
  HTTPClient http;
  WiFiClientSecure sec;
  WiFiClient plain;
  bool ok;
  if (strncmp(url, "https://", 8) == 0) { sec.setInsecure(); ok = http.begin(sec, url); }   // lásd a fejléc figyelmeztetését
  else                                    ok = http.begin(plain, url);
  if (!ok) { why = "ervenytelen URL"; return false; }
  http.setTimeout(8000);
  http.addHeader("Content-Type", ctype);
  if (bearer) http.addHeader("Authorization", String("Bearer ") + bearer);
  if (title) http.addHeader("Title", title);
  if (prio) http.addHeader("Priority", prio);
  const int code = http.POST(body);
  http.end();
  if (code >= 200 && code < 300) return true;
  why = code > 0 ? String("HTTP ") + code : String("kapcsolat: ") + HTTPClient::errorToString(code);
  return false;
}
