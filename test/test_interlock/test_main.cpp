#include <unity.h>
#include "InterlockGuard.h"
#include "Fakes.h"

void setUp() {}
void tearDown() {}

void test_allow_when_all_open_and_dead_time_over() {
  TEST_ASSERT_TRUE(InterlockGuard::allowClose(0, 0, 0, 5000, 5000));
}
void test_deny_when_other_commanded() {
  TEST_ASSERT_FALSE(InterlockGuard::allowClose(1, 0x01, 0, 9000, 5000));
}
void test_deny_when_other_feedback_closed() {
  TEST_ASSERT_FALSE(InterlockGuard::allowClose(2, 0, 0x02, 9000, 5000));   // összeheggedt K2
}
void test_deny_during_dead_time() {
  TEST_ASSERT_FALSE(InterlockGuard::allowClose(0, 0, 0, 1999, 5000));
}
void test_hard_minimum_cannot_be_configured_away() {
  TEST_ASSERT_FALSE(InterlockGuard::allowClose(0, 0, 0, 500, 0));          // minDead = 0 -> mégis 2 s
  TEST_ASSERT_TRUE(InterlockGuard::allowClose(0, 0, 0, 2000, 0));
}
void test_deny_invalid_source() {
  TEST_ASSERT_FALSE(InterlockGuard::allowClose(3, 0, 0, 9000, 5000));
}
void test_bank_never_closes_two() {
  FakeClock clk; FakeBank bank(clk);
  clk.t = 10000;
  TEST_ASSERT_TRUE(bank.close(0));
  TEST_ASSERT_FALSE(bank.close(1));
  TEST_ASSERT_FALSE(bank.close(2));
  TEST_ASSERT_EQUAL_UINT8(0x01, bank.commanded);
}
void test_bank_dead_time_after_open() {
  FakeClock clk; FakeBank bank(clk);
  clk.t = 10000; bank.close(0); clk.t = 10100; bank.tick();
  bank.openAll(); clk.t = 10500; bank.tick();
  TEST_ASSERT_FALSE(bank.close(1));            // 400 ms < 2 s
  clk.t = 12200; bank.tick();
  TEST_ASSERT_TRUE(bank.close(1));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_allow_when_all_open_and_dead_time_over);
  RUN_TEST(test_deny_when_other_commanded);
  RUN_TEST(test_deny_when_other_feedback_closed);
  RUN_TEST(test_deny_during_dead_time);
  RUN_TEST(test_hard_minimum_cannot_be_configured_away);
  RUN_TEST(test_deny_invalid_source);
  RUN_TEST(test_bank_never_closes_two);
  RUN_TEST(test_bank_dead_time_after_open);
  return UNITY_END();
}
