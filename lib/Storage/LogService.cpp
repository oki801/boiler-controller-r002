#include "LogService.h"

namespace { const char* kDir = "/log"; const char* kEvents = "/log/events.csv"; }

bool LogService::begin() {
  q_ = xQueueCreate(24, sizeof(Event));
  if (!LittleFS.exists(kDir)) LittleFS.mkdir(kDir);
  if (!LittleFS.exists(kEvents)) {
    File f = LittleFS.open(kEvents, "w");
    if (f) { f.println("ts,uptime_s,type,code,from,to,msg"); f.close(); }
  }
  lastFlushMs_ = millis();
  return true;
}

void LogService::eventThunk(const Event& e, void* ctx) {
  auto* self = static_cast<LogService*>(ctx);
  if (self->q_) xQueueSend(self->q_, &e, 0);               // nem blokkol; tele sor esetén eldob
}

static const char* typeName(EventType t) {
  switch (t) {
    case EventType::Switch:  return "SWITCH";
    case EventType::State:   return "STATE";
    case EventType::Error:   return "ERROR";
    case EventType::Command: return "CMD";
    default:                 return "SYS";
  }
}

void LogService::writeEvents(const char* line) {
  File f = LittleFS.open(kEvents, "a");
  if (!f) return;
  f.println(line);
  const size_t sz = f.size();
  f.close();
  if (sz > 200000) {                                        // egyszerű rotáció
    LittleFS.remove("/log/events_old.csv");
    LittleFS.rename(kEvents, "/log/events_old.csv");
    File n = LittleFS.open(kEvents, "w");
    if (n) { n.println("ts,uptime_s,type,code,from,to,msg"); n.close(); }
  }
}

// Az esemény olvasható sora a soros vonalra (a CSV-naplóval azonos eseményekből; ritkák, ezért nem terhelik a vonalat)
static void serialEvent(const Event& e, const char* when) {
  const unsigned long up = e.ms / 1000;
  const unsigned h = (unsigned)(up / 3600), m = (unsigned)(up / 60 % 60), s = (unsigned)(up % 60);
  char head[48];
  snprintf(head, sizeof(head), "[%s up %02u:%02u:%02u]", when, h, m, s);
  switch (e.type) {
    case EventType::Switch:
      Serial.printf("%s FORRAS-VALTAS %c -> %c  %s\n", head, e.from < 0 ? '-' : '1' + e.from, e.to < 0 ? '-' : '1' + e.to, e.msg);
      break;
    case EventType::Error: {
      const ErrorInfo& ei = errorInfo(e.code);
      Serial.printf("%s HIBA %s %s: %s %s\n", head, ei.code, e.raised ? "FELLEPETT" : "megszunt", ei.text, e.msg);
      break;
    }
    case EventType::State:   Serial.printf("%s ALLAPOT  %s\n", head, e.msg); break;
    case EventType::Command: Serial.printf("%s PARANCS  %s\n", head, e.msg); break;
    default:                 Serial.printf("%s RENDSZER %s\n", head, e.msg); break;
  }
}

void LogService::drainEvents() {
  Event e;
  char iso[32], line[kLineLen];
  while (q_ && xQueueReceive(q_, &e, 0) == pdTRUE) {
    time_.iso(iso, sizeof(iso));
    serialEvent(e, time_.isValid() ? iso : "ido?");
    const char* code = e.code == ErrorCode::None ? "" : errorInfo(e.code).code;
    snprintf(line, sizeof(line), "%s,%lu,%s,%s,%d,%d,%s", iso, (unsigned long)(e.ms / 1000), typeName(e.type),
             code, e.from < 0 ? 0 : e.from + 1, e.to < 0 ? 0 : e.to + 1, e.msg);
    writeEvents(line);          // az események ritkák, ezért azonnal íródnak (flash-kopás elfogadható)
  }
}

void LogService::addDataLine(const char* line) {
  Lock l(mtx_);
  if (n_ >= kLines) return;
  strlcpy(buf_[n_++], line, kLineLen);
}

void LogService::flush() {
  Lock l(mtx_);
  if (n_ == 0) return;
  char day[12], path[40];
  time_.dateStamp(day, sizeof(day));
  snprintf(path, sizeof(path), "%s/data_%s.csv", kDir, day);
  const bool isNew = !LittleFS.exists(path);
  File f = LittleFS.open(path, "a");
  if (f) {
    if (isNew) f.println("ts,uptime_s,active,fsm,temp_c,v,i,p_w,e_total_kwh,e_src1,e_src2,e_src3,flags");
    for (uint8_t i = 0; i < n_; ++i) f.println(buf_[i]);
    f.close();
  }
  n_ = 0;
  lastFlushMs_ = millis();
}

void LogService::tick(const Snapshot& s, const Config& cfg) {
  const uint32_t now = millis();
  drainEvents();

  if ((uint32_t)(now - lastSampleMs_) >= (uint32_t)cfg.logIntervalS * 1000UL) {
    lastSampleMs_ = now;
    char line[kLineLen];
    snprintf(line, sizeof(line), "%s,%lu,%d,%s,%.1f,%.1f,%.2f,%.0f,%.3f,%.3f,%.3f,%.3f,%lx%s", s.iso,
             (unsigned long)s.uptimeS, s.active < 0 ? 0 : s.active + 1, fsmName(s.fsm),
             s.tempValid ? (double)s.tempC : -127.0, (double)s.v, (double)s.i, (double)s.p, (double)s.eTotal,
             (double)s.eSrc[0], (double)s.eSrc[1], (double)s.eSrc[2], (unsigned long)s.errorMask,
             s.timeValid ? (s.timeEstimated ? ";E" : "") : ";T");
    addDataLine(line);
  }
  if (n_ >= kLines - 4 || (uint32_t)(now - lastFlushMs_) >= 300000UL) flush();

  if ((uint32_t)(now - lastPruneMs_) >= 3600000UL || lastPruneMs_ == 0) {
    lastPruneMs_ = now;
    prune(cfg.logRetentionDays);
  }
}

void LogService::prune(uint8_t retentionDays) {
  // A fájlnevek dátumot tartalmaznak: a legrégebbiek törlése, amíg túl sok van vagy 80% fölött a tárhely.
  for (int guard = 0; guard < 60; ++guard) {
    const size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
    storageLow_ = total && (used * 100 / total) >= 80;
    String oldest; int count = 0;
    File d = LittleFS.open(kDir);
    for (File f = d.openNextFile(); f; f = d.openNextFile()) {
      String n = f.name();
      if (!n.startsWith("data_")) continue;
      ++count;
      if (oldest.isEmpty() || n < oldest) oldest = n;
    }
    if (oldest.isEmpty() || (count <= retentionDays && !storageLow_)) break;
    LittleFS.remove(String(kDir) + "/" + oldest);
  }
}

bool LogService::safeName(const String& n) {
  if (n.isEmpty() || n.length() > 40) return false;
  if (n.indexOf('/') >= 0 || n.indexOf("..") >= 0 || n.indexOf('\\') >= 0) return false;
  return n.endsWith(".csv");
}

String LogService::eventsTail(size_t maxBytes) {
  File f = LittleFS.open(kEvents, "r");
  if (!f) return "";
  size_t sz = f.size();
  if (sz > maxBytes) { f.seek(sz - maxBytes); f.readStringUntil('\n'); }   // csonka első sor eldobása
  String s = f.readString();
  f.close();
  return s;
}

String LogService::listFiles() {
  String out = "[";
  File d = LittleFS.open(kDir);
  bool first = true;
  for (File f = d.openNextFile(); f; f = d.openNextFile()) {
    if (!first) out += ",";
    first = false;
    out += "{\"name\":\"" + String(f.name()) + "\",\"size\":" + String((unsigned)f.size()) + "}";
  }
  return out + "]";
}
