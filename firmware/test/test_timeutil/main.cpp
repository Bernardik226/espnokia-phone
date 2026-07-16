#include <unity.h>
#include "timeutil.h"

void test_reached_true_when_now_at_or_past_target() {
  TEST_ASSERT_TRUE(timeutil::reached(1000, 1000));
  TEST_ASSERT_TRUE(timeutil::reached(1001, 1000));
}
void test_reached_false_when_now_before_target() {
  TEST_ASSERT_FALSE(timeutil::reached(999, 1000));
}
void test_reached_survives_millis_rollover() {
  // now "voltou" pra perto de 0 depois do wrap de uint32_t; target ficou
  // perto do teto antigo — a subtracao sem sinal + cast pra int32_t continua
  // dando o resultado correto (ainda nao chegou)
  uint32_t target = 0xFFFFFFF0u;
  uint32_t now = 5u;  // 21 "ticks" depois do wrap
  TEST_ASSERT_TRUE(timeutil::reached(now, target));
  TEST_ASSERT_FALSE(timeutil::reached(target, target + 100));
}
void test_minuto_orders_within_same_day() {
  int32_t antes = timeutil::minuto(6, 15, 9, 30);   // 15/jun 09:30
  int32_t depois = timeutil::minuto(6, 15, 9, 31);  // 15/jun 09:31
  TEST_ASSERT_TRUE(depois > antes);
  TEST_ASSERT_EQUAL_INT32(1, depois - antes);
}
void test_minuto_matches_known_formula() {
  // ((mes*31+dia)*24+h)*60+m, aritmetica exata (nao e calendario real)
  TEST_ASSERT_EQUAL_INT32((((int32_t)3 * 31 + 4) * 24 + 5) * 60 + 6,
                          timeutil::minuto(3, 4, 5, 6));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_reached_true_when_now_at_or_past_target);
  RUN_TEST(test_reached_false_when_now_before_target);
  RUN_TEST(test_reached_survives_millis_rollover);
  RUN_TEST(test_minuto_orders_within_same_day);
  RUN_TEST(test_minuto_matches_known_formula);
  return UNITY_END();
}
