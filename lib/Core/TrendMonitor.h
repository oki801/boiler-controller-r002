#pragma once
// Vízkő-/fűtőelem-trendfigyelés. A vízkő közvetlenül nem mérhető, ezért két közvetett jelet követ,
// minden "tiszta" fűtési ciklusra (folyamatos nagy teljesítmény, vízvétel nélkül):
//  1) effektív hőkapacitás: Wh/°C  (az üledék/vízkő csökkenti a tényleges víztérfogatot),
//  2) normalizált fűtőteljesítmény: P * (230/U)^2  (a fűtőelem öregedése, részleges kiesése).
// A jelzés "gyanú", nem diagnózis; csak lassú trendre jó. Vízkőmentesítés után az alaphelyzetet nullázni kell.
#include <stdint.h>
#include <stddef.h>
#include <math.h>

constexpr uint8_t kTrendMaxRuns = 20;

struct TrendRun {
  float whPerC = 0;    // energia / hőmérséklet-emelkedés
  float pNorm  = 0;    // 230 V-ra normalizált átlagteljesítmény
  float peakC  = 0;    // a ciklus végén mért csúcshőmérséklet
  float dT     = 0;
  uint16_t durMin = 0;
};

struct TrendState {
  uint8_t  ver = 1, n = 0, head = 0;
  bool     baseReady = false;
  float    baseWh = 0, baseP = 0, basePeak = 0;
  TrendRun runs[kTrendMaxRuns];
};

class TrendMonitor {
 public:
  struct Params {
    float    minPowerW = 1500;          // ennél nagyobb teljesítmény = fűtés
    uint32_t startConfirmMs = 60000;    // ennyi ideig kell folyamatosan fűtenie az induláshoz
    uint32_t endConfirmMs = 30000;      // ennyi ideig kell leállnia a ciklus végéhez
    uint32_t minRunMs = 600000;         // 10 percnél rövidebb ciklus nem értékelhető
    float    minDeltaC = 8;             // legalább ennyit emelkedjen a hőmérséklet
    float    maxDipC = 1.5f;            // ennél nagyobb visszahűlés = vízvétel, a ciklus érvénytelen
    uint8_t  baselineRuns = 8;
    uint8_t  recentRuns = 8;
    float    thresholdPct = 10;
  };

  void setParams(const Params& p) { prm_ = p; }
  const Params& params() const { return prm_; }

  // 1 s-onként hívandó
  void update(uint32_t nowMs, bool tempValid, float tempC, bool pValid, float pW, float vV);

  size_t runCount() const { return st_.n; }
  TrendRun run(size_t i) const { return st_.runs[(st_.head + kTrendMaxRuns - st_.n + i) % kTrendMaxRuns]; }  // 0 = legrégebbi
  bool   running() const { return running_; }

  bool  baselineReady() const { return st_.baseReady; }
  bool  recentReady() const { return st_.baseReady && st_.n >= prm_.recentRuns; }
  float baselineWh() const { return st_.baseWh; }
  float baselinePower() const { return st_.baseP; }
  float recentWh() const { return recentMedian(0); }
  float recentPower() const { return recentMedian(1); }
  float recentPeak() const { return recentMedian(2); }
  float devPct() const { return recentReady() && st_.baseWh > 0 ? (recentWh() / st_.baseWh - 1.0f) * 100.0f : 0; }
  float powerDevPct() const { return recentReady() && st_.baseP > 0 ? (recentPower() / st_.baseP - 1.0f) * 100.0f : 0; }
  float peakDevC() const { return recentReady() ? recentPeak() - st_.basePeak : 0; }
  bool  scaleSuspect() const { return recentReady() && devPct() <= -prm_.thresholdPct; }
  bool  elementSuspect() const { return recentReady() && powerDevPct() <= -prm_.thresholdPct; }

  void reset() { st_ = TrendState(); running_ = false; aboveMs_ = 0; dirty_ = true; }   // vízkőmentesítés után
  bool takeDirty() { bool d = dirty_; dirty_ = false; return d; }
  const TrendState& state() const { return st_; }
  void importState(const TrendState& s) { if (s.ver == 1 && s.n <= kTrendMaxRuns && s.head < kTrendMaxRuns) st_ = s; }

 private:
  void startRun(float t);
  void finishRun(uint32_t nowMs);
  float recentMedian(int field) const;
  static float median(float* v, size_t n);

  Params prm_;
  TrendState st_;
  bool running_ = false, dirty_ = false, invalid_ = false;
  uint32_t lastMs_ = 0, aboveMs_ = 0, belowMs_ = 0, runMs_ = 0;
  float e_ = 0, sumP_ = 0, sumV_ = 0, t0_ = 0, peak_ = 0;
  uint32_t cnt_ = 0;
};

inline float TrendMonitor::median(float* v, size_t n) {
  for (size_t i = 1; i < n; ++i) { float x = v[i]; size_t j = i; while (j > 0 && v[j - 1] > x) { v[j] = v[j - 1]; --j; } v[j] = x; }
  return n % 2 ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

inline float TrendMonitor::recentMedian(int field) const {
  const size_t n = st_.n < prm_.recentRuns ? st_.n : prm_.recentRuns;
  if (!n) return 0;
  float v[kTrendMaxRuns];
  for (size_t i = 0; i < n; ++i) {
    const TrendRun r = run(st_.n - n + i);
    v[i] = field == 0 ? r.whPerC : field == 1 ? r.pNorm : r.peakC;
  }
  return median(v, n);
}

inline void TrendMonitor::startRun(float t) {
  running_ = true; invalid_ = false; belowMs_ = 0; runMs_ = 0;
  e_ = 0; sumP_ = 0; sumV_ = 0; cnt_ = 0; t0_ = t; peak_ = t;
}

inline void TrendMonitor::finishRun(uint32_t) {
  running_ = false; aboveMs_ = 0;
  const uint32_t durMs = runMs_ > belowMs_ ? runMs_ - belowMs_ : 0;
  const float dT = peak_ - t0_;
  if (invalid_ || durMs < prm_.minRunMs || dT < prm_.minDeltaC || cnt_ == 0) return;
  const float avgP = sumP_ / (float)cnt_, avgV = sumV_ / (float)cnt_;
  if (avgV < 180.0f || avgV > 260.0f) return;
  TrendRun r;
  r.whPerC = e_ / dT;
  r.pNorm = avgP * (230.0f / avgV) * (230.0f / avgV);
  r.peakC = peak_;
  r.dT = dT;
  r.durMin = (uint16_t)(durMs / 60000UL);
  st_.runs[st_.head] = r;
  st_.head = (uint8_t)((st_.head + 1) % kTrendMaxRuns);
  if (st_.n < kTrendMaxRuns) st_.n++;
  dirty_ = true;
  if (!st_.baseReady && st_.n >= prm_.baselineRuns) {       // az első N érvényes ciklus mediánja az alap
    float a[kTrendMaxRuns], b[kTrendMaxRuns], c[kTrendMaxRuns];
    for (size_t i = 0; i < st_.n; ++i) { const TrendRun x = run(i); a[i] = x.whPerC; b[i] = x.pNorm; c[i] = x.peakC; }
    st_.baseWh = median(a, st_.n); st_.baseP = median(b, st_.n); st_.basePeak = median(c, st_.n);
    st_.baseReady = true;
  }
}

inline void TrendMonitor::update(uint32_t nowMs, bool tempValid, float tempC, bool pValid, float pW, float vV) {
  uint32_t dt = lastMs_ ? (uint32_t)(nowMs - lastMs_) : 0;
  lastMs_ = nowMs;
  if (dt > 5000) dt = 5000;                                // kiesés/szünet utáni ugrás ellen
  const bool high = pValid && pW >= prm_.minPowerW;
  if (!running_) {
    if (high) { aboveMs_ += dt; if (aboveMs_ >= prm_.startConfirmMs && tempValid) startRun(tempC); }
    else aboveMs_ = 0;
    return;
  }
  runMs_ += dt;
  if (tempValid) {
    if (tempC > peak_) peak_ = tempC;
    if (peak_ - tempC > prm_.maxDipC) invalid_ = true;     // vízvétel / hideg beáramlás
  } else invalid_ = true;
  if (pValid && pW >= prm_.minPowerW * 0.5f) {
    belowMs_ = 0;
    e_ += pW * (float)dt / 3.6e6f;                          // W * ms -> Wh
    sumP_ += pW; sumV_ += vV; ++cnt_;
  } else {
    belowMs_ += dt;
    if (belowMs_ >= prm_.endConfirmMs) finishRun(nowMs);
  }
}
