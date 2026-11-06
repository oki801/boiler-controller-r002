#include <unity.h>
#include "Fakes.h"
#include "SwitchoverFsm.h"

struct Rig {
  FakeClock clk;
  FakeBank bank{clk};
  EventBus bus;
  ErrorTracker err{&bus, &clk};
  SwitchoverFsm fsm{bank, clk, &bus, &err};
  FsmInput in;
  uint8_t maxClosed = 0;

  void step(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 50) {
      clk.t += 50;
      bank.tick();
      fsm.update(in);
      if (bank.closedCount() > maxClosed) maxClosed = bank.closedCount();
      // Invariáns: soha nem lehet két kontaktor zárva
      TEST_ASSERT_TRUE_MESSAGE(bank.closedCount() <= 1, "ket kontaktor egyszerre zart");
      TEST_ASSERT_TRUE_MESSAGE(__builtin_popcount(bank.commanded) <= 1, "ket kontaktor parancsolva");
    }
  }
  void toIdle() { step(3500); }
};

void setUp() {}
void tearDown() {}

void test_boot_reaches_idle_with_all_open() {
  Rig r; r.toIdle();
  TEST_ASSERT_EQUAL(FsmState::Idle, r.fsm.state());
  TEST_ASSERT_EQUAL_UINT8(0, r.bank.closedCount());
  TEST_ASSERT_EQUAL_UINT8(0, r.bank.commanded);
}

void test_normal_connect_waits_dead_time() {
  Rig r; r.toIdle();
  r.in.wanted = 1;
  r.step(300);
  TEST_ASSERT_EQUAL(FsmState::DeadTime, r.fsm.state());
  TEST_ASSERT_EQUAL_UINT8(0, r.bank.commanded);             // még nem zárt
  r.step(5200);
  TEST_ASSERT_EQUAL(FsmState::Active, r.fsm.state());
  TEST_ASSERT_EQUAL_INT8(1, r.fsm.active());
  TEST_ASSERT_TRUE(r.bank.fb[1]);
}

void test_switch_over_on_source_loss() {
  Rig r; r.toIdle();
  r.in.wanted = 0; r.step(7000);
  TEST_ASSERT_EQUAL_INT8(0, r.fsm.active());
  r.in.activeOk = false; r.in.wanted = 2;                   // 1. forrás kiesett
  r.step(50);
  TEST_ASSERT_EQUAL_UINT8(0, r.bank.commanded);              // azonnal elengedett
  r.in.activeOk = true;
  r.step(300);
  TEST_ASSERT_EQUAL(FsmState::DeadTime, r.fsm.state());
  r.step(5500);
  TEST_ASSERT_EQUAL(FsmState::Active, r.fsm.state());
  TEST_ASSERT_EQUAL_INT8(2, r.fsm.active());
}

void test_min_dwell_blocks_preempt() {
  Rig r; r.toIdle();
  r.in.wanted = 1; r.step(7000);
  r.in.wanted = 0;                                          // magasabb prioritás megjelent
  r.step(60000);
  TEST_ASSERT_EQUAL(FsmState::Active, r.fsm.state());       // 120 s előtt marad
  TEST_ASSERT_EQUAL_INT8(1, r.fsm.active());
  r.step(70000);
  TEST_ASSERT_EQUAL_INT8(0, r.fsm.active());                // dwell letelt -> váltott
}

void test_open_now_disconnects() {
  Rig r; r.toIdle();
  r.in.wanted = 0; r.step(7000);
  r.in.openNow = true; r.in.wanted = kNone;
  r.step(500);
  TEST_ASSERT_EQUAL(FsmState::Idle, r.fsm.state());
  TEST_ASSERT_EQUAL_UINT8(0, r.bank.closedCount());
}

void test_welded_contact_faults_and_latches() {
  Rig r; r.toIdle();
  r.in.wanted = 0; r.step(7000);
  r.bank.stuck[0] = true;                                   // összeheggedt
  r.in.openNow = true; r.in.wanted = kNone;
  r.step(2000);
  TEST_ASSERT_EQUAL(FsmState::Fault, r.fsm.state());
  TEST_ASSERT_EQUAL(ErrorCode::E05, r.fsm.error());
  TEST_ASSERT_TRUE(r.fsm.latched());
  TEST_ASSERT_TRUE(r.err.active(ErrorCode::E05));
  // Új forrás kérése nem hozhat zárást
  r.in.openNow = false; r.in.wanted = 1;
  r.step(200000);
  TEST_ASSERT_EQUAL(FsmState::Fault, r.fsm.state());
  TEST_ASSERT_FALSE(r.bank.fb[1]);
}

void test_close_timeout_marks_source_failed() {
  Rig r; r.toIdle();
  r.bank.failClose[0] = true;
  r.in.wanted = 0; r.step(7500);
  TEST_ASSERT_EQUAL_INT8(0, r.fsm.consumeFailedSource());
  TEST_ASSERT_EQUAL_INT8(kNone, r.fsm.consumeFailedSource());   // egyszer fogyasztható
  TEST_ASSERT_TRUE(r.err.active(ErrorCode::E04));
  TEST_ASSERT_EQUAL_UINT8(0, r.bank.commanded);
  // Másik forrás kérésére tovább lép
  r.in.wanted = 1; r.step(9000);
  TEST_ASSERT_EQUAL(FsmState::Active, r.fsm.state());
  TEST_ASSERT_EQUAL_INT8(1, r.fsm.active());
  TEST_ASSERT_FALSE(r.err.active(ErrorCode::E04));
}

void test_two_feedbacks_closed_is_latched_e03() {
  Rig r; r.toIdle();
  r.bank.fb[0] = true; r.bank.fb[1] = true;                  // hardverhiba injektálása
  r.fsm.update(r.in);
  TEST_ASSERT_EQUAL(FsmState::Fault, r.fsm.state());
  TEST_ASSERT_EQUAL(ErrorCode::E03, r.fsm.error());
  TEST_ASSERT_TRUE(r.fsm.latched());
  TEST_ASSERT_EQUAL_UINT8(0, r.bank.commanded);
}

void test_ack_recovers_when_hardware_ok() {
  Rig r; r.toIdle();
  r.fsm.forceFault(ErrorCode::E10, true);
  r.step(100000);
  TEST_ASSERT_EQUAL(FsmState::Fault, r.fsm.state());         // zárolt: nem áll fel magától
  r.fsm.ackFault();
  r.step(2000);
  TEST_ASSERT_EQUAL(FsmState::Idle, r.fsm.state());
}

void test_nonlatched_fault_auto_retry() {
  Rig r; r.toIdle();
  r.fsm.forceFault(ErrorCode::E04, false);
  r.step(61000);
  TEST_ASSERT_TRUE(r.fsm.state() == FsmState::SelfTest || r.fsm.state() == FsmState::Idle);
}

void test_manual_bypass_blocks_and_recovers() {
  Rig r; r.toIdle();
  r.in.wanted = 0; r.step(7000);
  r.in.autoMode = false;
  r.step(200);
  TEST_ASSERT_EQUAL(FsmState::ManualBypass, r.fsm.state());
  TEST_ASSERT_EQUAL_UINT8(0, r.bank.commanded);
  r.step(5000);
  TEST_ASSERT_EQUAL_UINT8(0, r.bank.commanded);               // soha nem húz be
  r.in.autoMode = true;
  r.step(3000);
  TEST_ASSERT_EQUAL(FsmState::Idle, r.fsm.state() == FsmState::DeadTime ? FsmState::Idle : r.fsm.state());
}

void test_contactor_dropout_in_active() {
  Rig r; r.toIdle();
  r.in.wanted = 2; r.step(7000);
  r.bank.fb[2] = false; r.bank.failClose[2] = true;          // parancs nélkül elengedett
  r.step(50);
  TEST_ASSERT_EQUAL_INT8(2, r.fsm.consumeFailedSource());
  TEST_ASSERT_EQUAL_UINT8(0, r.bank.commanded);
}

void test_invariant_random_walk() {
  Rig r; r.toIdle();
  uint32_t seed = 12345;
  for (int k = 0; k < 4000; ++k) {
    seed = seed * 1664525u + 1013904223u;
    r.in.wanted   = (int8_t)((seed >> 24) % 4) - 1;          // -1..2
    r.in.activeOk = ((seed >> 20) & 7) != 0;
    r.in.openNow  = ((seed >> 16) & 31) == 0;
    r.step(100);
  }
  TEST_ASSERT_TRUE(r.maxClosed <= 1);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_boot_reaches_idle_with_all_open);
  RUN_TEST(test_normal_connect_waits_dead_time);
  RUN_TEST(test_switch_over_on_source_loss);
  RUN_TEST(test_min_dwell_blocks_preempt);
  RUN_TEST(test_open_now_disconnects);
  RUN_TEST(test_welded_contact_faults_and_latches);
  RUN_TEST(test_close_timeout_marks_source_failed);
  RUN_TEST(test_two_feedbacks_closed_is_latched_e03);
  RUN_TEST(test_ack_recovers_when_hardware_ok);
  RUN_TEST(test_nonlatched_fault_auto_retry);
  RUN_TEST(test_manual_bypass_blocks_and_recovers);
  RUN_TEST(test_contactor_dropout_in_active);
  RUN_TEST(test_invariant_random_walk);
  return UNITY_END();
}
