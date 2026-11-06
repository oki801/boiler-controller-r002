#include "MqttService.h"
#include "SnapshotJson.h"
#include <version.h>

void MqttService::begin() {
  q_ = xQueueCreate(16, sizeof(Event));
  mq_.setBufferSize(1536);
  mq_.setCallback([this](char* t, uint8_t* p, unsigned len) { onMessage(t, p, len); });
}

void MqttService::eventThunk(const Event& e, void* ctx) {
  auto* self = static_cast<MqttService*>(ctx);
  if (self->q_) xQueueSend(self->q_, &e, 0);
}

void MqttService::pub(const char* sub, const String& payload, bool retain) {
  mq_.publish(topic(sub).c_str(), payload.c_str(), retain);
}

bool MqttService::connect(const Config& c) {
  id_ = c.deviceId;
  base_ = c.mqttBase[0] ? String(c.mqttBase) : String("boiler/") + c.deviceId;
  mq_.setServer(c.mqttHost, c.mqttPort);
  const String will = topic("status");
  bool ok = c.mqttUser[0]
      ? mq_.connect(c.deviceId, c.mqttUser, c.mqttPass, will.c_str(), 1, true, "offline")
      : mq_.connect(c.deviceId, nullptr, nullptr, will.c_str(), 1, true, "offline");
  if (!ok) return false;
  pub("status", "online", true);
  mq_.subscribe(topic("cmd/#").c_str(), 1);
  discovery(c);
  lastState_ = "";                                       // az állapot újraközlése
  return true;
}

JsonDocument MqttService::baseDoc(const char* obj, const char* name) {
  JsonDocument d;
  d["name"] = String(name);          // másolat: a hívó ideiglenes String-jei nem élnek tovább
  d["unique_id"] = id_ + "_" + obj;
  d["availability_topic"] = topic("status");
  JsonObject dev = d["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add(id_);
  dev["name"] = "Bojlervezerlo";
  dev["manufacturer"] = "DIY";
  dev["model"] = "ESP32 3-halozatos";
  dev["sw_version"] = FW_BUILD;
  return d;
}

void MqttService::publishDoc(const char* component, const char* obj, JsonDocument& d) {
  String payload;
  serializeJson(d, payload);
  const String t = String("homeassistant/") + component + "/" + id_ + "/" + obj + "/config";
  mq_.publish(t.c_str(), payload.c_str(), true);
}

void MqttService::discovery(const Config& c) {
  auto sensor = [&](const char* obj, const char* name, const char* tpl, const char* unit, const char* dc,
                    const char* sc, const char* from) {
    JsonDocument d = baseDoc(obj, name);
    d["state_topic"] = topic(from);
    d["value_template"] = tpl;
    if (unit) d["unit_of_measurement"] = unit;
    if (dc) d["device_class"] = dc;
    if (sc) d["state_class"] = sc;
    publishDoc("sensor", obj, d);
  };
  sensor("temp",  "Bojler homerseklet", "{{ value_json.temp }}",  "°C", "temperature", "measurement", "telemetry");
  sensor("volt",  "Bojler feszultseg",  "{{ value_json.v }}",     "V",  "voltage",     "measurement", "telemetry");
  sensor("amp",   "Bojler aram",        "{{ value_json.i }}",     "A",  "current",     "measurement", "telemetry");
  sensor("power", "Bojler teljesitmeny","{{ value_json.p }}",     "W",  "power",       "measurement", "telemetry");
  sensor("energy","Bojler osszenergia", "{{ value_json.e }}",     "kWh","energy",      "total_increasing", "telemetry");
  sensor("active","Aktiv halozat",      "{{ value_json.active }}", nullptr, nullptr, nullptr, "state");
  sensor("scale", "Vizko index (hokapacitas valtozas)", "{{ value_json.scale_dev }}", "%", nullptr, "measurement", "telemetry");
  sensor("pdev",  "Futoelem teljesitmeny valtozas",      "{{ value_json.pnorm_dev }}", "%", nullptr, "measurement", "telemetry");
  sensor("heat",  "Futes allapot",       "{{ value_json.heat }}", nullptr, nullptr, nullptr, "state");
  sensor("fsm",   "Bojler allapot",     "{{ value_json.fsm }}",   nullptr, nullptr, nullptr, "state");

  for (uint8_t i = 0; i < kSources; ++i) {
    char obj[16], name[40], tpl[64], cmd[40];
    snprintf(obj, sizeof(obj), "e_src%d", i + 1);
    snprintf(name, sizeof(name), "Energia - %s", c.srcName[i]);
    snprintf(tpl, sizeof(tpl), "{{ value_json.e_src[%d] }}", i);
    sensor(obj, name, tpl, "kWh", "energy", "total_increasing", "telemetry");

    JsonDocument b = baseDoc((String("pres") + (i + 1)).c_str(), (String(c.srcName[i]) + " jelen").c_str());
    b["state_topic"] = topic("state");
    snprintf(tpl, sizeof(tpl), "{{ 'ON' if value_json.pres[%d] else 'OFF' }}", i);
    b["value_template"] = tpl;
    b["device_class"] = "power";
    publishDoc("binary_sensor", (String("pres") + (i + 1)).c_str(), b);

    JsonDocument en = baseDoc((String("enable") + (i + 1)).c_str(), (String(c.srcName[i]) + " engedelyezve").c_str());
    en["state_topic"] = topic("state");
    snprintf(tpl, sizeof(tpl), "{{ 'ON' if value_json.src[%d].enabled else 'OFF' }}", i);
    en["value_template"] = tpl;
    snprintf(cmd, sizeof(cmd), "%s/cmd/source/%d/enable/set", base_.c_str(), i + 1);
    en["command_topic"] = cmd;
    publishDoc("switch", (String("enable") + (i + 1)).c_str(), en);

    JsonDocument ex = baseDoc((String("extok") + (i + 1)).c_str(), (String(c.srcName[i]) + " kulso engedely").c_str());
    ex["state_topic"] = topic("state");
    snprintf(tpl, sizeof(tpl), "{{ 'ON' if value_json.src[%d].ext_ok else 'OFF' }}", i);
    ex["value_template"] = tpl;
    snprintf(cmd, sizeof(cmd), "%s/cmd/source/%d/ext_ok/set", base_.c_str(), i + 1);
    ex["command_topic"] = cmd;
    publishDoc("switch", (String("extok") + (i + 1)).c_str(), ex);
  }

  JsonDocument m = baseDoc("mode", "Uzemmod");
  m["state_topic"] = topic("state");
  m["value_template"] = "{{ value_json.mode }}";
  m["command_topic"] = topic("cmd/mode/set");
  JsonArray opts = m["options"].to<JsonArray>();
  opts.add("auto"); opts.add("force"); opts.add("off");
  publishDoc("select", "mode", m);

  JsonDocument fs = baseDoc("force", "Kenyszeritett halozat");
  fs["state_topic"] = topic("state");
  fs["value_template"] = "{{ value_json.forced if value_json.forced > 0 else '1' }}";
  fs["command_topic"] = topic("cmd/force_source/set");
  JsonArray fo = fs["options"].to<JsonArray>();
  fo.add("1"); fo.add("2"); fo.add("3");
  publishDoc("select", "force", fs);

  JsonDocument tg = baseDoc("target", "Celhomerseklet");
  tg["state_topic"] = topic("state");
  tg["value_template"] = "{{ value_json.target }}";
  tg["command_topic"] = topic("cmd/target/set");
  tg["min"] = 0; tg["max"] = 75; tg["step"] = 1;
  tg["unit_of_measurement"] = "°C";
  tg["device_class"] = "temperature";
  publishDoc("number", "target", tg);

  JsonDocument fl = baseDoc("fault", "Bojler hiba");
  fl["state_topic"] = topic("state");
  fl["value_template"] = "{{ 'ON' if value_json.fault is not none or value_json.errors | length > 0 else 'OFF' }}";
  fl["device_class"] = "problem";
  publishDoc("binary_sensor", "fault", fl);

  JsonDocument ak = baseDoc("ack", "Hiba nyugtazas");
  ak["command_topic"] = topic("cmd/fault/ack");
  ak["payload_press"] = "1";
  publishDoc("button", "ack", ak);
}

void MqttService::onMessage(const char* t, const uint8_t* payload, unsigned len) {
  char p[256];
  const unsigned n = len < sizeof(p) - 1 ? len : sizeof(p) - 1;
  memcpy(p, payload, n);
  p[n] = 0;
  const String prefix = base_ + "/cmd/";
  String tp = t;
  if (!tp.startsWith(prefix)) return;
  const String sub = tp.substring(prefix.length());
  const bool on = !strcasecmp(p, "ON") || !strcmp(p, "1") || !strcasecmp(p, "true");

  if (sub == "mode/set") {
    if (!strcasecmp(p, "auto")) { lastMode_ = Mode::Auto; sink_.setMode(Mode::Auto, kNone); }
    else if (!strcasecmp(p, "force")) { lastMode_ = Mode::Force; sink_.setMode(Mode::Force, lastForced_ > 0 ? lastForced_ - 1 : 0); }
    else if (!strcasecmp(p, "off")) { lastMode_ = Mode::Off; sink_.setMode(Mode::Off, kNone); }
  } else if (sub == "force_source/set") {
    const int s = atoi(p);
    if (s >= 1 && s <= kSources) { lastForced_ = (int8_t)s; sink_.setMode(Mode::Force, (int8_t)(s - 1)); }
  } else if (sub.startsWith("source/")) {
    const int s = sub.charAt(7) - '0';
    if (s < 1 || s > kSources) return;
    if (sub.endsWith("/enable/set")) {
      JsonDocument d;
      JsonArray a = d["src"].to<JsonArray>();
      for (int i = 0; i < kSources; ++i) { JsonObject o = a.add<JsonObject>(); if (i == s - 1) o["enabled"] = on; }
      char err[64];
      cfg_.patch(d.as<JsonVariantConst>(), err, sizeof(err));
    } else if (sub.endsWith("/ext_ok/set")) {
      sink_.setExtOk((uint8_t)(s - 1), on);
    }
  } else if (sub == "target/set") {
    JsonDocument d;
    d["target"] = (float)atof(p);
    char err[64];
    cfg_.patch(d.as<JsonVariantConst>(), err, sizeof(err));
  } else if (sub == "priority/set") {
    JsonDocument in;
    if (deserializeJson(in, p) == DeserializationError::Ok && in.is<JsonArray>()) {
      JsonDocument d;
      JsonArray a = d["src"].to<JsonArray>();
      for (int i = 0; i < kSources; ++i) { JsonObject o = a.add<JsonObject>(); if (!in[i].isNull()) o["priority"] = in[i].as<int>(); }
      char err[64];
      cfg_.patch(d.as<JsonVariantConst>(), err, sizeof(err));
    }
  } else if (sub == "config/set") {
    JsonDocument in;
    if (deserializeJson(in, p) == DeserializationError::Ok) {
      // MQTT-n át nem módosítható: hálózati adatok, jelszavak, azonosító (SF12/SF13)
      in.remove("wifi"); in.remove("mqtt"); in.remove("web"); in.remove("ota"); in.remove("deviceId");
      char err[64];
      cfg_.patch(in.as<JsonVariantConst>(), err, sizeof(err));
    }
  } else if (sub == "fault/ack") {
    sink_.ackFault();
  } else if (sub == "reboot") {
    sink_.reboot();
  }
}

void MqttService::loop(const Snapshot& s) {
  const Config c = cfg_.get();
  if (!c.mqttEnabled || WiFi.status() != WL_CONNECTED) {
    if (mq_.connected()) mq_.disconnect();
    return;
  }
  const uint32_t now = millis();
  if (!mq_.connected()) {
    if ((uint32_t)(now - lastAttemptMs_) >= backoffMs_) {
      lastAttemptMs_ = now;
      if (connect(c)) backoffMs_ = 5000;
      else backoffMs_ = backoffMs_ < 60000 ? backoffMs_ * 2 : 60000;       // exponenciális késleltetés
    }
    return;
  }
  mq_.loop();

  if ((uint32_t)(now - lastTeleMs_) >= 10000) {
    lastTeleMs_ = now;
    JsonDocument d; snapjson::telemetry(d, s);
    String o; serializeJson(d, o);
    pub("telemetry", o);
  }
  if ((uint32_t)(now - lastStateChkMs_) >= 1000) {
    lastStateChkMs_ = now;
    JsonDocument d; snapjson::state(d, s);
    String o; serializeJson(d, o);
    if (o != lastState_) { lastState_ = o; pub("state", o, true); }
  }
  Event e;
  while (q_ && xQueueReceive(q_, &e, 0) == pdTRUE) {
    JsonDocument d;
    d["ms"] = e.ms;
    d["msg"] = e.msg;
    if (e.type == EventType::Error) {
      d["code"] = errorInfo(e.code).code;
      d["sev"] = (int)errorInfo(e.code).sev;
      String o; serializeJson(d, o); pub("error", o);
    } else if (e.type == EventType::Switch) {
      d["from"] = e.from < 0 ? 0 : e.from + 1; d["to"] = e.to < 0 ? 0 : e.to + 1;
      String o; serializeJson(d, o); pub("event", o);
    }
  }
}
