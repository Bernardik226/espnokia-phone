#include <unity.h>
#include "btnlogic.h"

void test_press_after_debounce() {
  BtnState s;
  TEST_ASSERT_EQUAL_INT(EV_NONE, btn_step(s, true, 0));     // borda crua
  TEST_ASSERT_EQUAL_INT(EV_NONE, btn_step(s, true, 10));    // ainda instável
  TEST_ASSERT_EQUAL_INT(EV_PRESS, btn_step(s, true, 30));   // estável >25ms
  TEST_ASSERT_EQUAL_INT(EV_NONE, btn_step(s, true, 40));    // sem repetição
}
void test_bounce_is_ignored() {
  BtnState s;
  btn_step(s, true, 0);
  btn_step(s, false, 5);    // bounce reseta o timer
  btn_step(s, true, 12);
  TEST_ASSERT_EQUAL_INT(EV_NONE, btn_step(s, true, 30));    // 30-12 < 25
  TEST_ASSERT_EQUAL_INT(EV_PRESS, btn_step(s, true, 40));
}
void test_release_event() {
  BtnState s;
  btn_step(s, true, 0);
  btn_step(s, true, 30);                                     // PRESS
  btn_step(s, false, 100);
  TEST_ASSERT_EQUAL_INT(EV_RELEASE, btn_step(s, false, 130));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_press_after_debounce);
  RUN_TEST(test_bounce_is_ignored);
  RUN_TEST(test_release_event);
  return UNITY_END();
}
