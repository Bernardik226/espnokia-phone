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
void test_hhmm_format_pads_zeros() {
  char s[6];
  hhmm_format(7, 5, s);
  TEST_ASSERT_EQUAL_STRING("07:05", s);
  hhmm_format(23, 59, s);
  TEST_ASSERT_EQUAL_STRING("23:59", s);
  hhmm_format(0, 0, s);
  TEST_ASSERT_EQUAL_STRING("00:00", s);
}
void test_hhmm_format12_converts_and_flags_pm() {
  char s[6]; bool pm;
  hhmm_format12(0, 30, false, s, &pm);            // meia-noite em 12h -> 12:30 AM
  TEST_ASSERT_EQUAL_STRING("12:30", s);
  TEST_ASSERT_FALSE(pm);
  hhmm_format12(13, 5, false, s, &pm);            // 13h -> 01:05 PM
  TEST_ASSERT_EQUAL_STRING("01:05", s);
  TEST_ASSERT_TRUE(pm);
  hhmm_format12(12, 0, false, s, &pm);            // meio-dia -> 12:00 PM
  TEST_ASSERT_EQUAL_STRING("12:00", s);
  TEST_ASSERT_TRUE(pm);
  hhmm_format12(13, 5, true, s, &pm);             // 24h nao converte
  TEST_ASSERT_EQUAL_STRING("13:05", s);
}
void test_days_in_month_handles_leap_years() {
  TEST_ASSERT_EQUAL_UINT8(31, days_in_month(2026, 1));
  TEST_ASSERT_EQUAL_UINT8(28, days_in_month(2026, 2));
  TEST_ASSERT_EQUAL_UINT8(29, days_in_month(2028, 2));   // bissexto comum
  TEST_ASSERT_EQUAL_UINT8(28, days_in_month(2100, 2));   // secular nao bissexto
  TEST_ASSERT_EQUAL_UINT8(29, days_in_month(2000, 2));   // multiplo de 400
  TEST_ASSERT_EQUAL_UINT8(30, days_in_month(2026, 6));
  TEST_ASSERT_EQUAL_UINT8(31, days_in_month(2026, 12));
}
void test_date_weekday_known_dates() {
  TEST_ASSERT_EQUAL_UINT8(4, date_weekday(2026, 6, 11));  // quinta
  TEST_ASSERT_EQUAL_UINT8(0, date_weekday(2026, 6, 14));  // domingo
  TEST_ASSERT_EQUAL_UINT8(6, date_weekday(2000, 1, 1));   // sabado
  TEST_ASSERT_EQUAL_UINT8(2, date_weekday(2029, 2, 27));  // terca (m<3)
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_shows_noon);
  RUN_TEST(test_minutes_and_hours_advance);
  RUN_TEST(test_wraps_past_midnight);
  RUN_TEST(test_colon_blinks_each_second);
  RUN_TEST(test_hhmm_format_pads_zeros);
  RUN_TEST(test_hhmm_format12_converts_and_flags_pm);
  RUN_TEST(test_days_in_month_handles_leap_years);
  RUN_TEST(test_date_weekday_known_dates);
  return UNITY_END();
}
