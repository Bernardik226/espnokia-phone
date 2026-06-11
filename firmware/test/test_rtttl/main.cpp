#include <unity.h>
#include "rtttl.h"

void test_header_and_quarter_note() {
  Rtttl r; RtttlNote n;
  TEST_ASSERT_TRUE(r.begin("x:d=4,o=5,b=120:a"));
  TEST_ASSERT_TRUE(r.next(n));
  TEST_ASSERT_EQUAL_UINT16(880, n.freq_hz);   // a5: RTTTL o4 tem A=440 → a5 = 880Hz
  TEST_ASSERT_EQUAL_UINT16(500, n.dur_ms);    // semínima @120bpm = 500ms
  TEST_ASSERT_FALSE(r.next(n));
}
void test_explicit_duration_and_octave() {
  Rtttl r; RtttlNote n;
  r.begin("x:d=4,o=5,b=120:8e6");
  r.next(n);
  TEST_ASSERT_EQUAL_UINT16(1320, n.freq_hz);  // e6 = 330<<2 (E6 real 1318.5)
  TEST_ASSERT_EQUAL_UINT16(250, n.dur_ms);    // colcheia @120bpm
}
void test_sharp_pause_dotted() {
  Rtttl r; RtttlNote n;
  r.begin("x:d=8,o=5,b=120:f#,p,4c.");
  r.next(n); TEST_ASSERT_EQUAL_UINT16(740, n.freq_hz);   // f#5 = 370<<1
  r.next(n); TEST_ASSERT_EQUAL_UINT16(0, n.freq_hz);     // pausa
  r.next(n); TEST_ASSERT_EQUAL_UINT16(750, n.dur_ms);    // 4c. = 500*1.5
}
void test_nokia_tune_parses_all_13_notes() {
  Rtttl r; RtttlNote n; int count = 0;
  r.begin("Nokia:d=4,o=5,b=225:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a");
  while (r.next(n)) count++;
  TEST_ASSERT_EQUAL_INT(13, count);
}
void test_malformed_input_fails_safely() {
  Rtttl r; RtttlNote n;
  TEST_ASSERT_FALSE(r.begin("semicolon nenhum"));        // sem ':'
  TEST_ASSERT_FALSE(r.begin("x:d=,o=5,b=120:a"));        // d= sem dígito
  TEST_ASSERT_FALSE(r.begin("x:d=4,o=5,b=:a"));          // b= sem dígito
  r.begin("x:d=4,o=5,b=120:a9");                          // oitava fora da faixa
  TEST_ASSERT_FALSE(r.next(n));
  r.begin("x:d=4,o=5,b=120:p#");                          // p# vira pausa normal
  TEST_ASSERT_TRUE(r.next(n));
  TEST_ASSERT_EQUAL_UINT16(0, n.freq_hz);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_header_and_quarter_note);
  RUN_TEST(test_explicit_duration_and_octave);
  RUN_TEST(test_sharp_pause_dotted);
  RUN_TEST(test_nokia_tune_parses_all_13_notes);
  RUN_TEST(test_malformed_input_fails_safely);
  return UNITY_END();
}
