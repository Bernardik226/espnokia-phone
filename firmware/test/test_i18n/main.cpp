#include <unity.h>
#include <string.h>
#include "i18n.h"

void setUp() { i18n_set(LANG_PT); }
void tearDown() {}

void test_every_string_exists_in_every_lang() {
  // nenhuma celula da tabela pode ficar vazia (menos STR_NONE)
  for (uint8_t l = 0; l < LANG_COUNT; l++) {
    i18n_set((Lang)l);
    for (uint8_t s = STR_NONE + 1; s < STR_COUNT; s++)
      TEST_ASSERT_GREATER_THAN(0, (int)strlen(tr((StrId)s)));
  }
}

void test_switching_lang_switches_strings() {
  i18n_set(LANG_PT);
  TEST_ASSERT_EQUAL_STRING("Voltar", tr(STR_BACK));
  i18n_set(LANG_FI);
  TEST_ASSERT_EQUAL_STRING("Takaisin", tr(STR_BACK));
  TEST_ASSERT_EQUAL(LANG_FI, i18n_lang());
}

void test_accents_are_utf8() {
  // "Relógio" tem o ó multi-byte: prova que a tabela esta em UTF-8
  i18n_set(LANG_PT);
  const char* s = tr(STR_APP_CLOCK);
  TEST_ASSERT_EQUAL_UINT8(0xC3, (uint8_t)s[3]);  // inicio do "ó"
  TEST_ASSERT_EQUAL_UINT8(0xB3, (uint8_t)s[4]);
}

void test_out_of_range_is_safe() {
  i18n_set((Lang)200);
  TEST_ASSERT_EQUAL(LANG_PT, i18n_lang());
  TEST_ASSERT_EQUAL_STRING("", tr((StrId)250));
}

void test_day_names_follow_lang() {
  i18n_set(LANG_PT);
  TEST_ASSERT_EQUAL_STRING("Domingo", day_name(0));
  i18n_set(LANG_DE);
  TEST_ASSERT_EQUAL_STRING("Mittwoch", day_name(3));
  TEST_ASSERT_EQUAL_STRING("Sonntag", day_name(7));  // wrap modulo 7
}

void test_lang_names_are_native() {
  TEST_ASSERT_EQUAL_STRING("Suomi", lang_name(LANG_FI));
  TEST_ASSERT_EQUAL_STRING("Português", lang_name(LANG_PT));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_string_exists_in_every_lang);
  RUN_TEST(test_switching_lang_switches_strings);
  RUN_TEST(test_accents_are_utf8);
  RUN_TEST(test_out_of_range_is_safe);
  RUN_TEST(test_day_names_follow_lang);
  RUN_TEST(test_lang_names_are_native);
  return UNITY_END();
}
