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
void test_repeat_after_hold() {
  BtnState s;
  btn_step(s, true, 0);
  btn_step(s, true, 30);                                     // PRESS
  TEST_ASSERT_FALSE(btn_repeat(s, 30));                      // agenda o 1o
  TEST_ASSERT_FALSE(btn_repeat(s, 400));                     // antes do delay
  TEST_ASSERT_TRUE(btn_repeat(s, 480));                      // 30+450
  TEST_ASSERT_FALSE(btn_repeat(s, 500));                     // antes do ritmo
  TEST_ASSERT_TRUE(btn_repeat(s, 590));                      // 480+110
}
void test_repeat_stops_on_release() {
  BtnState s;
  btn_step(s, true, 0);
  btn_step(s, true, 30);                                     // PRESS
  btn_repeat(s, 30);
  TEST_ASSERT_TRUE(btn_repeat(s, 480));
  btn_step(s, false, 500);
  btn_step(s, false, 530);                                   // RELEASE
  TEST_ASSERT_FALSE(btn_repeat(s, 600));                     // soltou: para
  btn_step(s, true, 700);
  btn_step(s, true, 730);                                    // PRESS de novo
  TEST_ASSERT_FALSE(btn_repeat(s, 730));                     // delay reinicia
  TEST_ASSERT_FALSE(btn_repeat(s, 1100));
  TEST_ASSERT_TRUE(btn_repeat(s, 1180));                     // 730+450
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_press_after_debounce);
  RUN_TEST(test_bounce_is_ignored);
  RUN_TEST(test_release_event);
  RUN_TEST(test_repeat_after_hold);
  RUN_TEST(test_repeat_stops_on_release);
  return UNITY_END();
}
