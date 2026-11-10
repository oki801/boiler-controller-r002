#include "TrendService.h"

void TrendService::begin() {
  prefs_.begin("trend", false);
  TrendState st;
  // isKey: friss flash-en a kulcs hianyzik, ez nem hiba (a getBytesLength "nvs_get_blob len fail" hibat naplozna)
  if (prefs_.isKey("st") && prefs_.getBytesLength("st") == sizeof(TrendState) && prefs_.getBytes("st", &st, sizeof(st)) == sizeof(st)) {
    Lock l(mtx_);
    mon_.importState(st);
  }
}

void TrendService::tick(const Snapshot& s, const Config& c) {
  if (!c.trendEnabled) return;
  Lock l(mtx_);
  TrendMonitor::Params p = mon_.params();
  p.thresholdPct = c.trendThresholdPct;
  mon_.setParams(p);
  mon_.update(millis(), s.tempValid, s.tempC, s.pzemValid, s.p, s.v);
  if (mon_.takeDirty()) prefs_.putBytes("st", &mon_.state(), sizeof(TrendState));   // ciklusonként legfeljebb egyszer ír
}

void TrendService::reset() {
  Lock l(mtx_);
  mon_.reset();
  mon_.takeDirty();
  prefs_.remove("st");
}

TrendService::Summary TrendService::summary() const {
  Lock l(mtx_);
  Summary r;
  r.baseReady = mon_.baselineReady();
  r.ready = mon_.recentReady();
  r.runs = (uint8_t)mon_.runCount();
  r.devPct = mon_.devPct();
  r.powerDevPct = mon_.powerDevPct();
  r.scaleSuspect = mon_.scaleSuspect();
  r.elementSuspect = mon_.elementSuspect();
  return r;
}

String TrendService::json() const {
  Lock l(mtx_);
  JsonDocument d;
  d["runs"] = (uint32_t)mon_.runCount();
  d["need"] = mon_.params().baselineRuns;
  d["base_ready"] = mon_.baselineReady();
  d["ready"] = mon_.recentReady();
  d["threshold"] = mon_.params().thresholdPct;
  if (mon_.baselineReady()) {
    d["base_wh"] = mon_.baselineWh();  d["base_p"] = mon_.baselinePower();
  }
  if (mon_.recentReady()) {
    d["recent_wh"] = mon_.recentWh();  d["recent_p"] = mon_.recentPower();
    d["dev_pct"] = mon_.devPct();      d["p_dev_pct"] = mon_.powerDevPct();
    d["peak_dev_c"] = mon_.peakDevC();
  }
  d["scale_suspect"] = mon_.scaleSuspect();
  d["element_suspect"] = mon_.elementSuspect();
  d["running"] = mon_.running();
  JsonArray h = d["history"].to<JsonArray>();
  for (size_t i = 0; i < mon_.runCount(); ++i) {
    const TrendRun r = mon_.run(i);
    JsonObject o = h.add<JsonObject>();
    o["wh"] = r.whPerC;  o["p"] = r.pNorm;  o["peak"] = r.peakC;  o["dt"] = r.dT;  o["min"] = r.durMin;
  }
  String out;
  serializeJson(d, out);
  return out;
}
