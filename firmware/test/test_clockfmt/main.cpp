#include <unity.h>
#include <string.h>
#include "clockfmt.h"

void test_boot_shows_noon() {
  char s[6]; bool c;
  clock_format(0, s, &c);
  TEST_ASSERT_EQUAL_STRING("12:00", s);
  TEST_ASSERT_TRUE(c);
}
void test_minutes_and_hours_advance() {
  char s[6]; bool c;
  clock_format(61UL * 60 * 1000, s, &c);      // +1h01
  TEST_ASSERT_EQUAL_STRING("13:01", s);
}
void test_wraps_past_midnight() {
  char s[6]; bool c;
  clock_format((12UL * 60 + 1) * 60 * 1000, s, &c);  // 12:00 + 12h01
  TEST_ASSERT_EQUAL_STRING("00:01", s);
}
void test_colon_blinks_each_second() {
  char s[6]; bool c;
  clock_format(1000, s, &c);
  TEST_ASSERT_FALSE(c);                       // segundo ímpar: apagado
  clock_format(2000, s, &c);
  TEST_ASSERT_TRUE(c);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_shows_noon);
  RUN_TEST(test_minutes_and_hours_advance);
  RUN_TEST(test_wraps_past_midnight);
  RUN_TEST(test_colon_blinks_each_second);
  return UNITY_END();
}
