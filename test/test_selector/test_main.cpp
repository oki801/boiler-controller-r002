#include <unity.h>
#include "SourceSelector.h"
#include "GuaranteeLogic.h"

static SelectorInput base() {
  SelectorInput in;
  in.nowMs = 1000000; in.timeValid = true; in.minuteOfDay = 12 * 60; in.weekday = 2;
  for (uint8_t i = 0; i < kSources; ++i) {
    in.cfg[i].priority = i + 1;
    in.st[i].present = true;
    in.st[i].stableSinceMs = 0;     // régóta stabil
    in.st[i].extOk = true;
  }
  return in;
}

void setUp() {}
void tearDown() {}

void test_highest_priority_wins() {
  SelectorInput in = base();
  TEST_ASSERT_EQUAL_INT8(0, selectSource(in));
  in.cfg[0].priority = 3; in.cfg[2].priority = 1;
  TEST_ASSERT_EQUAL_INT8(2, selectSource(in));
}

void test_unavailable_source_skipped() {
  SelectorInput in = base();
  in.st[0].present = false;
  TEST_ASSERT_EQUAL_INT8(1, selectSource(in));
  in.cfg[1].enabled = false;
  TEST_ASSERT_EQUAL_INT8(2, selectSource(in));
  in.st[2].faulted = true;
  TEST_ASSERT_EQUAL_INT8(kNone, selectSource(in));
}

void test_stability_required_for_new_source() {
  SelectorInput in = base();
  in.st[0].stableSinceMs = in.nowMs - 5000;        // 5 s < 30 s
  TEST_ASSERT_EQUAL_INT8(1, selectSource(in));
  in.st[0].stableSinceMs = in.nowMs - 31000;
  TEST_ASSERT_EQUAL_INT8(0, selectSource(in));
}

void test_overnight_window() {
  SelectorInput in = base();
  in.cfg[0].schedFrom = 22 * 60; in.cfg[0].schedTo = 6 * 60;   // 22:00-06:00
  in.minuteOfDay = 23 * 60;  TEST_ASSERT_TRUE(inWindow(in.cfg[0], true, in.minuteOfDay, in.weekday));
  in.minuteOfDay = 3 * 60;   TEST_ASSERT_TRUE(inWindow(in.cfg[0], true, in.minuteOfDay, in.weekday));
  in.minuteOfDay = 12 * 60;  TEST_ASSERT_FALSE(inWindow(in.cfg[0], true, in.minuteOfDay, in.weekday));
  TEST_ASSERT_EQUAL_INT8(1, selectSource(in));      // délben a 2. forrás nyer
}

void test_day_mask() {
  SelectorInput in = base();
  in.cfg[0].dayMask = 0x03;                          // hétfő, kedd
  in.weekday = 2;                                    // szerda
  TEST_ASSERT_FALSE(inWindow(in.cfg[0], true, in.minuteOfDay, in.weekday));
  in.weekday = 1;
  TEST_ASSERT_TRUE(inWindow(in.cfg[0], true, in.minuteOfDay, in.weekday));
}

void test_no_time_policy() {
  SelectorInput in = base();
  in.timeValid = false;
  in.cfg[0].schedFrom = 0; in.cfg[0].schedTo = 12 * 60;   // beosztott -> idő nélkül tiltott
  TEST_ASSERT_EQUAL_INT8(1, selectSource(in));            // korlátozás nélküli forrás igen
}

void test_ext_ok() {
  SelectorInput in = base();
  in.cfg[0].needExtOk = true; in.st[0].extOk = false;
  TEST_ASSERT_EQUAL_INT8(1, selectSource(in));
  in.st[0].extOk = true;
  TEST_ASSERT_EQUAL_INT8(0, selectSource(in));
}

void test_active_stays_until_lost() {
  SelectorInput in = base();
  in.active = 1; in.activeSinceMs = in.nowMs - 1000; // frissen aktív
  TEST_ASSERT_EQUAL_INT8(1, selectSource(in));       // 1. forrás jobb, de a tartózkodás nem telt le
  TEST_ASSERT_TRUE(activeStillOk(in));
  in.st[1].present = false;                          // kiesik
  TEST_ASSERT_FALSE(activeStillOk(in));
  TEST_ASSERT_EQUAL_INT8(0, selectSource(in));
}

void test_preempt_after_dwell() {
  SelectorInput in = base();
  in.active = 1; in.activeSinceMs = in.nowMs - 130000;   // > 120 s
  TEST_ASSERT_EQUAL_INT8(0, selectSource(in));
  in.preempt = false;
  TEST_ASSERT_EQUAL_INT8(1, selectSource(in));
}

void test_no_downgrade_while_active_ok() {
  SelectorInput in = base();
  in.active = 0; in.activeSinceMs = 0;
  TEST_ASSERT_EQUAL_INT8(0, selectSource(in));          // alacsonyabb prioritású nem vált
}

void test_millis_wraparound() {
  SelectorInput in = base();
  in.nowMs = 1000;                                       // túlcsordulás után
  for (uint8_t i = 1; i < kSources; ++i) in.st[i].stableSinceMs = 0xFFFFFFFFu - 40000u;
  in.st[0].stableSinceMs = 0xFFFFFFFFu - 40000u;         // ~41 s-sel ezelőtt
  TEST_ASSERT_EQUAL_INT8(0, selectSource(in));
  in.st[0].stableSinceMs = 0xFFFFFFFFu - 10000u;         // ~11 s
  TEST_ASSERT_EQUAL_INT8(1, selectSource(in));
}

void test_forced() {
  SelectorInput in = base();
  TEST_ASSERT_EQUAL_INT8(2, selectForced(in, 2));
  in.st[2].present = false;
  TEST_ASSERT_EQUAL_INT8(kNone, selectForced(in, 2));
  in.st[2].present = true; in.cfg[2].enabled = false;
  TEST_ASSERT_EQUAL_INT8(kNone, selectForced(in, 2));
  TEST_ASSERT_EQUAL_INT8(kNone, selectForced(in, 7));
}

void test_cap_blocked_source_skipped() {
  SelectorInput in = base();
  in.st[0].capBlocked = true;
  TEST_ASSERT_EQUAL_INT8(1, selectSource(in));
  in.st[1].capBlocked = true;
  TEST_ASSERT_EQUAL_INT8(2, selectSource(in));
}
void test_cap_drops_active_source() {
  SelectorInput in = base();
  in.active = 1; in.activeSinceMs = 0;
  TEST_ASSERT_TRUE(activeStillOk(in));
  in.st[1].capBlocked = true;                               // elérte a hőfokhatárt
  TEST_ASSERT_FALSE(activeStillOk(in));
  TEST_ASSERT_EQUAL_INT8(0, selectSource(in));
}
void test_cap_blocks_forced_source() {
  SelectorInput in = base();
  in.st[2].capBlocked = true;
  TEST_ASSERT_EQUAL_INT8(kNone, selectForced(in, 2));
}
void test_guarantee_overrides_schedule_and_ext() {
  SelectorInput in = base();
  in.cfg[0].needExtOk = true; in.st[0].extOk = false;        // napelem: külső engedély nélkül tiltott
  in.cfg[1].schedFrom = 0; in.cfg[1].schedTo = 6 * 60;       // 2. forrás csak éjjel, most dél van
  in.st[2].present = false;
  TEST_ASSERT_EQUAL_INT8(kNone, selectSource(in));
  in.guaranteeActive = true;
  TEST_ASSERT_EQUAL_INT8(0, selectSource(in));               // garancia: a legmagasabb prioritású használható
  in.cfg[0].guarantee = false;
  TEST_ASSERT_EQUAL_INT8(1, selectSource(in));               // amelyik forrás nem engedi, azt nem használja
}
void test_guarantee_does_not_bypass_presence_or_cap() {
  SelectorInput in = base();
  in.guaranteeActive = true;
  in.cfg[0].schedFrom = 0; in.cfg[0].schedTo = 6 * 60;
  in.st[0].present = false;
  TEST_ASSERT_FALSE(usable(in, 0));
  in.st[0].present = true; in.st[0].capBlocked = true;
  TEST_ASSERT_FALSE(usable(in, 0));
}

// ---- GuaranteeLogic / CapGate ----
void test_guarantee_window() {
  GuaranteeCfg g;                                           // 19:00 kész, 90 perc előfűtés, 23:00-ig
  TEST_ASSERT_FALSE(inGuaranteeWindow(g, 17 * 60 + 29));
  TEST_ASSERT_TRUE(inGuaranteeWindow(g, 17 * 60 + 30));
  TEST_ASSERT_TRUE(inGuaranteeWindow(g, 22 * 60 + 59));
  TEST_ASSERT_FALSE(inGuaranteeWindow(g, 23 * 60));
  TEST_ASSERT_FALSE(inGuaranteeWindow(g, 12 * 60));          // napközben nincs garancia
}
void test_guarantee_window_across_midnight() {
  GuaranteeCfg g; g.readyAtMin = 23 * 60; g.leadMin = 60; g.untilMin = 1 * 60;
  TEST_ASSERT_TRUE(inGuaranteeWindow(g, 22 * 60 + 30));
  TEST_ASSERT_TRUE(inGuaranteeWindow(g, 0 * 60 + 30));
  TEST_ASSERT_FALSE(inGuaranteeWindow(g, 2 * 60));
}
void test_guarantee_hysteresis() {
  GuaranteeCfg g; const uint16_t evening = 19 * 60;
  TEST_ASSERT_TRUE (updateGuarantee(g, false, true, evening, true, 40.0f));
  TEST_ASSERT_FALSE(updateGuarantee(g, false, true, evening, true, 46.0f));   // nincs szükség
  TEST_ASSERT_TRUE (updateGuarantee(g, true,  true, evening, true, 46.0f));   // már fűt: 48-ig megy
  TEST_ASSERT_FALSE(updateGuarantee(g, true,  true, evening, true, 48.5f));
}
void test_guarantee_off_without_time_or_temp() {
  GuaranteeCfg g; const uint16_t evening = 19 * 60;
  TEST_ASSERT_FALSE(updateGuarantee(g, false, false, evening, true, 30.0f));  // nincs idő
  TEST_ASSERT_FALSE(updateGuarantee(g, false, true, evening, false, 0.0f));   // nincs hőmérséklet
  g.enabled = false;
  TEST_ASSERT_FALSE(updateGuarantee(g, false, true, evening, true, 30.0f));
}
void test_cap_gate_hysteresis() {
  SourceCfg c[kSources]; c[0].maxTempC = 70; c[1].maxTempC = 55; c[2].maxTempC = 55;
  CapGate gate;
  gate.update(54.0f, true, c, 3.0f);   TEST_ASSERT_FALSE(gate.blocked(1));
  gate.update(55.0f, true, c, 3.0f);   TEST_ASSERT_TRUE(gate.blocked(1));  TEST_ASSERT_FALSE(gate.blocked(0));
  gate.update(53.0f, true, c, 3.0f);   TEST_ASSERT_TRUE(gate.blocked(1));  // még a hiszterézisen belül
  gate.update(52.0f, true, c, 3.0f);   TEST_ASSERT_FALSE(gate.blocked(1));
  gate.update(71.0f, true, c, 3.0f);   TEST_ASSERT_TRUE(gate.blocked(0));
  gate.update(0.0f, false, c, 3.0f);   TEST_ASSERT_FALSE(gate.blocked(0)); // nincs mérés: nem blokkol
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_highest_priority_wins);
  RUN_TEST(test_unavailable_source_skipped);
  RUN_TEST(test_stability_required_for_new_source);
  RUN_TEST(test_overnight_window);
  RUN_TEST(test_day_mask);
  RUN_TEST(test_no_time_policy);
  RUN_TEST(test_ext_ok);
  RUN_TEST(test_active_stays_until_lost);
  RUN_TEST(test_preempt_after_dwell);
  RUN_TEST(test_no_downgrade_while_active_ok);
  RUN_TEST(test_millis_wraparound);
  RUN_TEST(test_forced);
  RUN_TEST(test_cap_blocked_source_skipped);
  RUN_TEST(test_cap_drops_active_source);
  RUN_TEST(test_cap_blocks_forced_source);
  RUN_TEST(test_guarantee_overrides_schedule_and_ext);
  RUN_TEST(test_guarantee_does_not_bypass_presence_or_cap);
  RUN_TEST(test_guarantee_window);
  RUN_TEST(test_guarantee_window_across_midnight);
  RUN_TEST(test_guarantee_hysteresis);
  RUN_TEST(test_guarantee_off_without_time_or_temp);
  RUN_TEST(test_cap_gate_hysteresis);
  return UNITY_END();
}
