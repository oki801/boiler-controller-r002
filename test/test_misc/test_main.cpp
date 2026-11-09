#include <unity.h>
#include "PresenceFilter.h"
#include "EnergyAccumulator.h"
#include "Config.h"
#include "SafetyMonitor.h"
#include "ErrorTracker.h"
#include "HistoryBuffer.h"
#include "NotifyPolicy.h"
#include "TrendMonitor.h"
#include "Thermostat.h"
#include "SensorFreshness.h"

void setUp() {}
void tearDown() {}

// ---- PresenceFilter ----
void test_presence_basic() {
  PresenceFilter f(3, 2000);
  f.update(100, 0);   TEST_ASSERT_FALSE(f.present());
  f.update(200, 10);  TEST_ASSERT_TRUE(f.present());
  TEST_ASSERT_EQUAL_UINT32(200, f.stableSinceMs());
}
void test_presence_short_dropout_ignored() {
  PresenceFilter f(3, 2000);
  f.update(0, 10); f.update(100, 10);
  f.update(1000, 0); f.update(1900, 0);                      // < 2 s
  TEST_ASSERT_TRUE(f.present());
  f.update(2200, 10);                                        // visszatért
  TEST_ASSERT_EQUAL_UINT32(0, f.stableSinceMs());            // a stabilitás nem nullázódott
}
void test_presence_loss_after_timeout() {
  PresenceFilter f(3, 2000);
  f.update(0, 10);
  f.update(2100, 0);
  TEST_ASSERT_FALSE(f.present());
}
void test_presence_noise_below_threshold() {
  PresenceFilter f(3, 2000);
  f.update(0, 2);
  TEST_ASSERT_FALSE(f.present());
}

// ---- EnergyAccumulator ----
void test_energy_attribution() {
  EnergyAccumulator e;
  e.update(0, true, 10.00f);          // első minta: alapérték
  e.update(0, true, 10.50f);
  e.update(2, true, 11.00f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, e.srcKwh(0));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, e.srcKwh(1));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, e.srcKwh(2));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, e.totalKwh());
}
void test_energy_counter_reset_and_wrap() {
  EnergyAccumulator e;
  e.update(1, true, 5.0f);
  e.update(1, true, 0.2f);                                   // PZEM nullázva
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, e.srcKwh(1));
  EnergyAccumulator w;
  w.update(0, true, 9999.9f);
  w.update(0, true, 0.3f);                                   // átfordulás
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4f, w.srcKwh(0));
}
void test_energy_ignores_invalid() {
  EnergyAccumulator e;
  e.update(0, true, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, e.update(0, false, 99.0f));
  e.update(0, true, 1.2f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, e.srcKwh(0));
}
void test_energy_discard_absurd_jump() {
  EnergyAccumulator e;
  e.update(0, true, 1.0f);
  e.update(0, true, 500.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, e.srcKwh(0));
}

// ---- Config ----
void test_config_defaults_valid() {
  Config c; char err[64];
  TEST_ASSERT_TRUE(validateConfig(c, err, sizeof(err)));
}
void test_config_rejects_short_dead_time() {
  Config c; char err[64];
  c.timing.deadMs = 1000;
  TEST_ASSERT_FALSE(validateConfig(c, err, sizeof(err)));
}
void test_config_rejects_bad_priority_and_id() {
  Config c; char err[64];
  c.src[0].priority = 9;
  TEST_ASSERT_FALSE(validateConfig(c, err, sizeof(err)));
  Config d; strcpy(d.deviceId, "Bad ID");
  TEST_ASSERT_FALSE(validateConfig(d, err, sizeof(err)));
}
void test_config_rejects_bad_voltage_range() {
  Config c; char err[64];
  c.safety.vMin = 260; c.safety.vMax = 250;
  TEST_ASSERT_FALSE(validateConfig(c, err, sizeof(err)));
}

// ---- SafetyMonitor ----
void test_overtemp_hysteresis() {
  EventBus bus; ErrorTracker et(&bus);
  SafetyMonitor s(&et);
  SafetyInput in; in.tempValid = true;
  in.tempC = 74; s.update(in);  TEST_ASSERT_FALSE(s.overTemp());
  in.tempC = 76; s.update(in);  TEST_ASSERT_TRUE(s.overTemp());
  in.tempC = 72; s.update(in);  TEST_ASSERT_TRUE(s.overTemp());    // hiszterézis
  in.tempC = 69; s.update(in);  TEST_ASSERT_FALSE(s.overTemp());
}
void test_overtemp_ignored_when_sensor_invalid() {
  SafetyMonitor s; SafetyInput in; in.tempValid = false; in.tempC = 200;
  s.update(in);
  TEST_ASSERT_FALSE(s.overTemp());
}
void test_voltage_fault_blocks_source() {
  SafetyMonitor s; SafetyInput in;
  in.fsmActive = true; in.active = 1; in.pzemValid = true; in.v = 180;
  for (uint32_t t = 0; t <= 6000; t += 500) { in.now = t; s.update(in); }
  TEST_ASSERT_TRUE(s.sourceBlocked(1, 6000));
  TEST_ASSERT_FALSE(s.sourceBlocked(0, 6000));
  TEST_ASSERT_FALSE(s.sourceBlocked(1, 6000 + 61000));            // a tartás lejár
}
void test_source_failures_latch() {
  SafetyMonitor s;
  s.markSourceFailed(0, 1000); s.markSourceFailed(0, 70000);
  TEST_ASSERT_FALSE(s.sourceLatched(0));
  s.markSourceFailed(0, 140000);
  TEST_ASSERT_TRUE(s.sourceLatched(0));
  TEST_ASSERT_TRUE(s.sourceBlocked(0, 999999999));
  s.ack();
  TEST_ASSERT_FALSE(s.sourceBlocked(0, 999999999));
}
void test_switch_limit() {
  SafetyMonitor s; SafetyCfg c; c.maxSwitchesPerHour = 3; s.setConfig(c, 60000);
  s.noteSwitch(1000); s.noteSwitch(2000);
  TEST_ASSERT_FALSE(s.switchLimited(3000));
  s.noteSwitch(3000);
  TEST_ASSERT_TRUE(s.switchLimited(4000));
  TEST_ASSERT_FALSE(s.switchLimited(3600000UL + 5000));          // egy óra után lejár
}

// ---- ErrorTracker ----
void test_error_tracker_edges() {
  struct Ctx { int n = 0; } ctx;
  EventBus bus;
  bus.subscribe([](const Event&, void* c) { ((Ctx*)c)->n++; }, &ctx);
  ErrorTracker et(&bus);
  TEST_ASSERT_TRUE(et.set(ErrorCode::E01, true));
  TEST_ASSERT_FALSE(et.set(ErrorCode::E01, true));               // nincs új él
  TEST_ASSERT_TRUE(et.active(ErrorCode::E01));
  TEST_ASSERT_TRUE(et.set(ErrorCode::E01, false));
  TEST_ASSERT_EQUAL(2, ctx.n);
  et.set(ErrorCode::E03, true);
  TEST_ASSERT_TRUE(et.worst() == Severity::Critical);
}

// ---- HistoryBuffer ----
static HistSample hs(int16_t t, uint16_t p, uint8_t a, uint16_t v = 2300) { HistSample s; s.tempX10 = t; s.pW = p; s.vX10 = v; s.active = a; s.flags = 0; return s; }

void test_history_ring_wraps_and_keeps_order() {
  HistSample mem[4]; HistoryBuffer h(mem, 4);
  for (int i = 1; i <= 6; ++i) h.push(hs(i * 10, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(4, h.size());
  TEST_ASSERT_EQUAL_INT16(30, h.at(0).tempX10);          // a legrégebbi megmaradt minta
  TEST_ASSERT_EQUAL_INT16(60, h.at(3).tempX10);          // a legfrissebb
}
void test_history_empty_downsample() {
  HistSample mem[4]; HistoryBuffer h(mem, 4); HistoryBuffer::Bucket b[4]; size_t per = 9;
  TEST_ASSERT_EQUAL_UINT32(0, h.downsample(4, b, &per));
  TEST_ASSERT_EQUAL_UINT32(0, per);
}
void test_history_downsample_averages() {
  HistSample mem[16]; HistoryBuffer h(mem, 16); HistoryBuffer::Bucket b[8]; size_t per = 0;
  for (int i = 0; i < 8; ++i) h.push(hs(400 + i * 10, 1000 + i * 100, 1));
  const size_t n = h.downsample(4, b, &per);
  TEST_ASSERT_EQUAL_UINT32(4, n);
  TEST_ASSERT_EQUAL_UINT32(2, per);
  TEST_ASSERT_EQUAL_INT16(405, b[0].tempX10);             // (400+410)/2
  TEST_ASSERT_EQUAL_UINT16(1050, b[0].pW);
  TEST_ASSERT_EQUAL_INT16(465, b[3].tempX10);             // (460+470)/2
}
void test_history_dominant_source_and_invalid_temp() {
  HistSample mem[16]; HistoryBuffer h(mem, 16); HistoryBuffer::Bucket b[2]; size_t per = 0;
  h.push(hs(kHistNoTemp, 0, 2)); h.push(hs(500, 100, 2)); h.push(hs(520, 100, 3));
  const size_t n = h.downsample(1, b, &per);
  TEST_ASSERT_EQUAL_UINT32(1, n);
  TEST_ASSERT_EQUAL_UINT32(3, per);
  TEST_ASSERT_EQUAL_UINT8(2, b[0].active);                // 2 db a 2. hálózat, 1 db a 3.
  TEST_ASSERT_EQUAL_INT16(510, b[0].tempX10);             // az érvénytelen minta kimarad az átlagból
}
void test_history_all_invalid_temp_stays_invalid() {
  HistSample mem[4]; HistoryBuffer h(mem, 4); HistoryBuffer::Bucket b[2]; size_t per = 0;
  h.push(hs(kHistNoTemp, 0, 0)); h.push(hs(kHistNoTemp, 0, 0));
  h.downsample(2, b, &per);
  TEST_ASSERT_EQUAL_INT16(kHistNoTemp, b[0].tempX10);
}
void test_history_more_buckets_than_samples() {
  HistSample mem[8]; HistoryBuffer h(mem, 8); HistoryBuffer::Bucket b[8]; size_t per = 0;
  h.push(hs(300, 0, 1)); h.push(hs(310, 0, 1));
  TEST_ASSERT_EQUAL_UINT32(2, h.downsample(8, b, &per));
  TEST_ASSERT_EQUAL_UINT32(1, per);
}

// ---- ErrorTracker: raised jelző ----
void test_error_event_raised_flag() {
  struct Ctx { bool lastRaised = true; int n = 0; } ctx;
  EventBus bus;
  bus.subscribe([](const Event& e, void* c) { auto* x = (Ctx*)c; x->lastRaised = e.raised; x->n++; }, &ctx);
  ErrorTracker et(&bus);
  et.set(ErrorCode::E05, true);   TEST_ASSERT_TRUE(ctx.lastRaised);
  et.set(ErrorCode::E05, false);  TEST_ASSERT_FALSE(ctx.lastRaised);
  TEST_ASSERT_EQUAL(2, ctx.n);
}

// ---- Config: új mezők ----
void test_config_rejects_unreachable_guarantee() {
  Config c; char err[80];
  for (auto& s : c.src) s.maxTempC = 40;                        // egyik forrás sem éri el a 45+3 °C-ot
  TEST_ASSERT_FALSE(validateConfig(c, err, sizeof(err)));
  c.guar.enabled = false;
  TEST_ASSERT_TRUE(validateConfig(c, err, sizeof(err)));
}
void test_config_rejects_cap_above_safety_limit() {
  Config c; char err[80];
  c.src[0].maxTempC = 80;                                       // a védelmi határ 75 °C
  TEST_ASSERT_FALSE(validateConfig(c, err, sizeof(err)));
}
void test_config_notify_validation() {
  Config c; char err[80];
  c.notifyKind = 1;                                             // Telegram: token és chat nélkül hibás
  TEST_ASSERT_FALSE(validateConfig(c, err, sizeof(err)));
  strcpy(c.notifyToken, "123:abc"); strcpy(c.notifyChat, "42");
  TEST_ASSERT_TRUE(validateConfig(c, err, sizeof(err)));
  c.notifyKind = 2; strcpy(c.notifyUrl, "ftp://x");
  TEST_ASSERT_FALSE(validateConfig(c, err, sizeof(err)));
  strcpy(c.notifyUrl, "http://192.168.1.10/hook");
  TEST_ASSERT_TRUE(validateConfig(c, err, sizeof(err)));
}

// ---- NotifyPolicy ----
void test_notify_severity_filter() {
  NotifyPolicy n;
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E05, true, Severity::Critical, 1000) == NotifyPolicy::Action::SendRaised);
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E01, true, Severity::Critical, 1000) == NotifyPolicy::Action::None);   // csak figyelmeztetés
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E01, true, Severity::Warn, 1000) == NotifyPolicy::Action::SendRaised);
}
void test_notify_cooldown_and_clear() {
  NotifyPolicy n;
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E03, true, Severity::Critical, 0) == NotifyPolicy::Action::SendRaised);
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E03, true, Severity::Critical, 60000) == NotifyPolicy::Action::None);          // ismétlés-védelem
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E03, false, Severity::Critical, 120000) == NotifyPolicy::Action::SendCleared);
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E03, false, Severity::Critical, 130000) == NotifyPolicy::Action::None);        // nincs dupla megszűnt
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E03, true, Severity::Critical, 200000) == NotifyPolicy::Action::None);         // gyors újra-fellépés: még a cooldownban
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E03, true, Severity::Critical, NotifyPolicy::kCooldownMs + 1) == NotifyPolicy::Action::SendRaised);
}
void test_notify_no_cleared_without_raised_sent() {
  NotifyPolicy n;
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E05, false, Severity::Critical, 0) == NotifyPolicy::Action::None);
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E01, true, Severity::Critical, 0) == NotifyPolicy::Action::None);
  TEST_ASSERT_TRUE(n.decide(ErrorCode::E01, false, Severity::Critical, 10) == NotifyPolicy::Action::None);          // sosem jeleztük
}

// ---- TrendMonitor ----
static void sim1s(TrendMonitor& m, uint32_t& now, float t, float p, float v, int sec) {
  for (int i = 0; i < sec; ++i) { m.update(now, true, t, true, p, v); now += 1000; }
}
// Egy fűtési ciklus szimulációja: whPerC = a "víz" effektív hőkapacitása, a hőmérséklet ennek megfelelően emelkedik.
static void heatRun(TrendMonitor& m, uint32_t& now, float whPerC, float dT = 25.0f, float vV = 230.0f, bool draw = false) {
  const float p = 3000.0f * (vV / 230.0f) * (vV / 230.0f);
  float t = 30.0f;
  sim1s(m, now, t, 40, vV, 120);                              // nyugalmi állapot
  const float target = 30.0f + dT + 4.0f;                     // az első perc és a lag miatt ráhagyás
  int s = 0;
  while (t < target) {
    m.update(now, true, t, true, p, vV); now += 1000;
    t += (p / 3600.0f) / whPerC;
    if (draw && ++s == 600) t -= 4.0f;                        // vízvétel: hirtelen visszahűlés
  }
  sim1s(m, now, t, 0, vV, 90);                                // termosztát lekapcsol
}
void test_trend_baseline_and_no_alarm_when_stable() {
  TrendMonitor m; uint32_t now = 1000;
  for (int i = 0; i < 8; ++i) heatRun(m, now, 140.0f);
  TEST_ASSERT_EQUAL_UINT32(8, m.runCount());
  TEST_ASSERT_TRUE(m.baselineReady());
  TEST_ASSERT_FLOAT_WITHIN(4.0f, 140.0f, m.baselineWh());
  for (int i = 0; i < 8; ++i) heatRun(m, now, 140.0f);
  TEST_ASSERT_TRUE(m.recentReady());
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 0.0f, m.devPct());
  TEST_ASSERT_FALSE(m.scaleSuspect());
  TEST_ASSERT_FALSE(m.elementSuspect());
}
void test_trend_detects_capacity_loss() {
  TrendMonitor m; uint32_t now = 1000;
  for (int i = 0; i < 8; ++i) heatRun(m, now, 140.0f);
  for (int i = 0; i < 8; ++i) heatRun(m, now, 120.0f);        // kb. -14% effektív hőkapacitás
  TEST_ASSERT_TRUE(m.devPct() < -10.0f);
  TEST_ASSERT_TRUE(m.scaleSuspect());
  TEST_ASSERT_FALSE(m.elementSuspect());
}
void test_trend_small_drift_below_threshold() {
  TrendMonitor m; uint32_t now = 1000;
  for (int i = 0; i < 8; ++i) heatRun(m, now, 140.0f);
  for (int i = 0; i < 8; ++i) heatRun(m, now, 131.0f);        // kb. -6%
  TEST_ASSERT_FALSE(m.scaleSuspect());
}
void test_trend_voltage_normalization_prevents_false_element_alarm() {
  TrendMonitor m; uint32_t now = 1000;
  for (int i = 0; i < 8; ++i) heatRun(m, now, 140.0f, 25.0f, 230.0f);
  for (int i = 0; i < 8; ++i) heatRun(m, now, 140.0f, 25.0f, 212.0f);   // alacsony hálózati feszültség, kisebb teljesítmény
  TEST_ASSERT_FALSE(m.elementSuspect());
}
void test_trend_detects_element_power_drop() {
  TrendMonitor m; uint32_t now = 1000;
  for (int i = 0; i < 8; ++i) heatRun(m, now, 140.0f);
  // a fűtőelem részben kiesett: kisebb teljesítmény ugyanazon a feszültségen (V nem változik, P igen)
  for (int i = 0; i < 8; ++i) {
    float t = 30.0f; const float p = 2400.0f;
    sim1s(m, now, t, 40, 230.0f, 120);
    while (t < 59.0f) { m.update(now, true, t, true, p, 230.0f); now += 1000; t += (p / 3600.0f) / 140.0f; }
    sim1s(m, now, t, 0, 230.0f, 90);
  }
  TEST_ASSERT_TRUE(m.powerDevPct() < -10.0f);
  TEST_ASSERT_TRUE(m.elementSuspect());
  TEST_ASSERT_FALSE(m.scaleSuspect());
}
void test_trend_ignores_runs_with_water_draw_and_short_runs() {
  TrendMonitor m; uint32_t now = 1000;
  heatRun(m, now, 140.0f, 25.0f, 230.0f, true);               // vízvétel a ciklus közben
  TEST_ASSERT_EQUAL_UINT32(0, m.runCount());
  heatRun(m, now, 140.0f, 4.0f);                              // csak pár fokot emelkedett
  TEST_ASSERT_EQUAL_UINT32(0, m.runCount());
}
void test_trend_reset_after_descaling() {
  TrendMonitor m; uint32_t now = 1000;
  for (int i = 0; i < 8; ++i) heatRun(m, now, 140.0f);
  m.reset();
  TEST_ASSERT_EQUAL_UINT32(0, m.runCount());
  TEST_ASSERT_FALSE(m.baselineReady());
  TEST_ASSERT_FALSE(m.scaleSuspect());
}
void test_trend_state_roundtrip() {
  TrendMonitor a; uint32_t now = 1000;
  for (int i = 0; i < 9; ++i) heatRun(a, now, 140.0f);
  TrendMonitor b; b.importState(a.state());
  TEST_ASSERT_EQUAL_UINT32(a.runCount(), b.runCount());
  TEST_ASSERT_TRUE(b.baselineReady());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, a.baselineWh(), b.baselineWh());
}

// ---- Thermostat ----
void test_thermostat_hysteresis_cycle() {
  Thermostat t;                                                     // cél 55, hiszterézis 2
  TEST_ASSERT_TRUE (t.update(55, 2, true, 50.0f, false));  TEST_ASSERT_TRUE(t.phase() == HeatPhase::Heating);
  TEST_ASSERT_TRUE (t.update(55, 2, true, 54.0f, false));  TEST_ASSERT_TRUE(t.phase() == HeatPhase::Heating);   // a sávban fűt tovább
  TEST_ASSERT_FALSE(t.update(55, 2, true, 55.0f, false));  TEST_ASSERT_TRUE(t.phase() == HeatPhase::Reached);
  TEST_ASSERT_FALSE(t.update(55, 2, true, 54.0f, false));  TEST_ASSERT_TRUE(t.phase() == HeatPhase::Waiting);   // a sávban vár
  TEST_ASSERT_FALSE(t.update(55, 2, true, 53.1f, false));  TEST_ASSERT_TRUE(t.phase() == HeatPhase::Waiting);
  TEST_ASSERT_TRUE (t.update(55, 2, true, 52.9f, false));  TEST_ASSERT_TRUE(t.phase() == HeatPhase::Heating);   // cél - hiszterézis alatt újraindul
}
void test_thermostat_off_when_target_zero_even_with_guarantee() {
  Thermostat t;
  TEST_ASSERT_FALSE(t.update(0, 2, true, 20.0f, true));
  TEST_ASSERT_TRUE(t.phase() == HeatPhase::Off);
}
void test_thermostat_guarantee_overrides_low_target() {
  Thermostat t;
  TEST_ASSERT_FALSE(t.update(40, 2, true, 45.0f, false));           // cél 40, a víz 45: nem fűt
  TEST_ASSERT_TRUE (t.update(40, 2, true, 45.0f, true));            // este a garancia fűt, a cél alacsonyabb
}
void test_thermostat_invalid_temperature_does_not_block() {
  Thermostat t;
  TEST_ASSERT_TRUE(t.update(55, 2, false, 0.0f, false));
  TEST_ASSERT_TRUE(t.phase() == HeatPhase::Unknown);
}
void test_thermostat_changing_target_reacts() {
  Thermostat t;
  t.update(50, 2, true, 50.0f, false);                              // Reached
  TEST_ASSERT_FALSE(t.demand());
  TEST_ASSERT_TRUE(t.update(60, 2, true, 50.0f, false));            // a cél 60-ra nőtt: azonnal fűt
}
void test_config_target_validation() {
  Config c; char err[80];
  c.targetC = 5;   TEST_ASSERT_FALSE(validateConfig(c, err, sizeof(err)));      // 10 alatt értelmetlen
  c.targetC = 20;  TEST_ASSERT_TRUE (validateConfig(c, err, sizeof(err)));      // a dashboard csúszka 1-60 közti értékeket enged
  c.targetC = 80;  TEST_ASSERT_FALSE(validateConfig(c, err, sizeof(err)));      // a védelmi határ 75
  c.targetC = 0;   TEST_ASSERT_TRUE (validateConfig(c, err, sizeof(err)));      // 0 = kikapcsolva
  c.targetC = 60;  c.hystC = 0.2f; TEST_ASSERT_FALSE(validateConfig(c, err, sizeof(err)));
  c.hystC = 2.0f;  TEST_ASSERT_TRUE (validateConfig(c, err, sizeof(err)));
}

// ---- SensorFreshness ----
void test_freshness_not_stale_before_first_reading() {
  SensorFreshness f;
  f.update(100000, 0);
  TEST_ASSERT_FALSE(f.stale(100000, 5000));                 // seq == 0: még nem volt olvasás, nem elavulás
}
void test_freshness_stale_when_seq_stops() {
  SensorFreshness f;
  f.update(1000, 1);
  f.update(2000, 2);
  f.update(3000, 3);
  TEST_ASSERT_FALSE(f.stale(8000, 5000));                   // pontosan 5000 ms: még friss
  TEST_ASSERT_TRUE(f.stale(8001, 5000));                    // 5001 ms: elavult
}
void test_freshness_recovers_when_seq_moves() {
  SensorFreshness f;
  f.update(1000, 1);
  f.update(9000, 1);
  TEST_ASSERT_TRUE(f.stale(9000, 5000));
  f.update(9100, 2);
  TEST_ASSERT_FALSE(f.stale(9100, 5000));
}
void test_freshness_millis_wraparound() {
  SensorFreshness f;
  f.update(0xFFFFFF00u, 7);
  TEST_ASSERT_FALSE(f.stale(0x00000400u, 5000));            // átfordult óra, ~1,3 s telt el
  TEST_ASSERT_TRUE(f.stale(0x00002000u, 5000));             // ~8,4 s telt el
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_presence_basic);
  RUN_TEST(test_presence_short_dropout_ignored);
  RUN_TEST(test_presence_loss_after_timeout);
  RUN_TEST(test_presence_noise_below_threshold);
  RUN_TEST(test_energy_attribution);
  RUN_TEST(test_energy_counter_reset_and_wrap);
  RUN_TEST(test_energy_ignores_invalid);
  RUN_TEST(test_energy_discard_absurd_jump);
  RUN_TEST(test_config_defaults_valid);
  RUN_TEST(test_config_rejects_short_dead_time);
  RUN_TEST(test_config_rejects_bad_priority_and_id);
  RUN_TEST(test_config_rejects_bad_voltage_range);
  RUN_TEST(test_overtemp_hysteresis);
  RUN_TEST(test_overtemp_ignored_when_sensor_invalid);
  RUN_TEST(test_voltage_fault_blocks_source);
  RUN_TEST(test_source_failures_latch);
  RUN_TEST(test_switch_limit);
  RUN_TEST(test_error_tracker_edges);
  RUN_TEST(test_history_ring_wraps_and_keeps_order);
  RUN_TEST(test_history_empty_downsample);
  RUN_TEST(test_history_downsample_averages);
  RUN_TEST(test_history_dominant_source_and_invalid_temp);
  RUN_TEST(test_history_all_invalid_temp_stays_invalid);
  RUN_TEST(test_history_more_buckets_than_samples);
  RUN_TEST(test_error_event_raised_flag);
  RUN_TEST(test_config_rejects_unreachable_guarantee);
  RUN_TEST(test_config_rejects_cap_above_safety_limit);
  RUN_TEST(test_config_notify_validation);
  RUN_TEST(test_notify_severity_filter);
  RUN_TEST(test_notify_cooldown_and_clear);
  RUN_TEST(test_notify_no_cleared_without_raised_sent);
  RUN_TEST(test_trend_baseline_and_no_alarm_when_stable);
  RUN_TEST(test_trend_detects_capacity_loss);
  RUN_TEST(test_trend_small_drift_below_threshold);
  RUN_TEST(test_trend_voltage_normalization_prevents_false_element_alarm);
  RUN_TEST(test_trend_detects_element_power_drop);
  RUN_TEST(test_trend_ignores_runs_with_water_draw_and_short_runs);
  RUN_TEST(test_trend_reset_after_descaling);
  RUN_TEST(test_trend_state_roundtrip);
  RUN_TEST(test_thermostat_hysteresis_cycle);
  RUN_TEST(test_thermostat_off_when_target_zero_even_with_guarantee);
  RUN_TEST(test_thermostat_guarantee_overrides_low_target);
  RUN_TEST(test_thermostat_invalid_temperature_does_not_block);
  RUN_TEST(test_thermostat_changing_target_reacts);
  RUN_TEST(test_config_target_validation);
  RUN_TEST(test_freshness_not_stale_before_first_reading);
  RUN_TEST(test_freshness_stale_when_seq_stops);
  RUN_TEST(test_freshness_recovers_when_seq_moves);
  RUN_TEST(test_freshness_millis_wraparound);
  return UNITY_END();
}
