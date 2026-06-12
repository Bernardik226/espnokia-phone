#include <unity.h>
#include <string.h>
#include "claudemodel.h"

using namespace claude;

// ---- humor ----
void test_dias_civis_ancoras() {
  TEST_ASSERT_EQUAL_INT32(0, dias_civis(1970, 1, 1));
  TEST_ASSERT_EQUAL_INT32(1, dias_civis(1970, 1, 2));
  // bissexto: 29/02 existe em 1972
  TEST_ASSERT_EQUAL_INT32(2, dias_civis(1972, 3, 1) - dias_civis(1972, 2, 28));
  TEST_ASSERT_EQUAL_INT32(365, dias_civis(1971, 1, 1));
}
void test_epoch_min_consistente() {
  int64_t meia_noite = epoch_min(2026, 6, 12, 0, 0);
  TEST_ASSERT_EQUAL_INT64(meia_noite + 8 * 60 + 30, epoch_min(2026, 6, 12, 8, 30));
  TEST_ASSERT_EQUAL_INT64(meia_noite + 24 * 60, epoch_min(2026, 6, 13, 0, 0));
}
void test_humor_limiar_de_24h() {
  int64_t agora = epoch_min(2026, 6, 12, 12, 0);
  TEST_ASSERT_EQUAL(H_FELIZ, humor(agora, agora - (24 * 60 - 1), 12));
  TEST_ASSERT_EQUAL(H_NEUTRO, humor(agora, agora - 24 * 60, 12));
}
void test_humor_limiar_de_3_dias() {
  int64_t agora = epoch_min(2026, 6, 12, 12, 0);
  TEST_ASSERT_EQUAL(H_NEUTRO, humor(agora, agora - 3 * 24 * 60, 12));
  TEST_ASSERT_EQUAL(H_CARENTE, humor(agora, agora - (3 * 24 * 60 + 1), 12));
}
void test_humor_nunca_conversou_e_neutro() {
  TEST_ASSERT_EQUAL(H_NEUTRO, humor(epoch_min(2026, 6, 12, 12, 0), 0, 12));
  // RTC regrediu (perdeu bateria) com NVS guardada: nao pode virar FELIZ
  TEST_ASSERT_EQUAL(H_NEUTRO, humor(epoch_min(2000, 1, 1, 12, 0),
                                    epoch_min(2026, 6, 12, 12, 0), 12));
}
void test_humor_janela_de_sono() {
  int64_t agora = epoch_min(2026, 6, 12, 23, 30);
  TEST_ASSERT_EQUAL(H_DORMINDO, humor(agora, agora - 10, 23));
  TEST_ASSERT_EQUAL(H_DORMINDO, humor(agora, agora - 10, 3));
  TEST_ASSERT_EQUAL(H_DORMINDO, humor(agora, agora - 10, 6));
  TEST_ASSERT_EQUAL(H_FELIZ, humor(agora, agora - 10, 7));
  TEST_ASSERT_EQUAL(H_FELIZ, humor(agora, agora - 10, 22));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dias_civis_ancoras);
  RUN_TEST(test_epoch_min_consistente);
  RUN_TEST(test_humor_limiar_de_24h);
  RUN_TEST(test_humor_limiar_de_3_dias);
  RUN_TEST(test_humor_nunca_conversou_e_neutro);
  RUN_TEST(test_humor_janela_de_sono);
  return UNITY_END();
}
