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

// ---- maquina de estados ----
void test_maquina_caminho_feliz() {
  Maquina m;
  maquina_init(m, 1000);
  TEST_ASSERT_EQUAL(E_PET, m.st);
  TEST_ASSERT_TRUE(maquina_evento(m, EV_OK_APERTA, 2000));
  TEST_ASSERT_EQUAL(E_OUVINDO, m.st);
  TEST_ASSERT_TRUE(maquina_evento(m, EV_OK_SOLTA, 5000));
  TEST_ASSERT_EQUAL(E_PENSANDO, m.st);
  TEST_ASSERT_TRUE(maquina_evento(m, EV_RESPOSTA, 8000));
  TEST_ASSERT_EQUAL(E_FALANDO, m.st);
  TEST_ASSERT_TRUE(maquina_evento(m, EV_VOLTA, 9000));
  TEST_ASSERT_EQUAL(E_PET, m.st);
}
void test_maquina_falha_vai_pra_falando() {
  Maquina m;
  maquina_init(m, 0);
  maquina_evento(m, EV_OK_APERTA, 0);
  maquina_evento(m, EV_OK_SOLTA, 100);
  TEST_ASSERT_TRUE(maquina_evento(m, EV_FALHA, 200));
  TEST_ASSERT_EQUAL(E_FALANDO, m.st);
}
void test_maquina_estoura_15s_ouvindo() {
  Maquina m;
  maquina_init(m, 0);
  maquina_evento(m, EV_OK_APERTA, 1000);
  TEST_ASSERT_FALSE(maquina_tick(m, 1000 + kOuvindoMaxMs - 1));
  TEST_ASSERT_TRUE(maquina_tick(m, 1000 + kOuvindoMaxMs));
  TEST_ASSERT_EQUAL(E_PENSANDO, m.st);
}
void test_maquina_estoura_60s_falando() {
  Maquina m;
  maquina_init(m, 0);
  maquina_evento(m, EV_OK_APERTA, 0);
  maquina_evento(m, EV_OK_SOLTA, 100);
  maquina_evento(m, EV_RESPOSTA, 200);
  TEST_ASSERT_FALSE(maquina_tick(m, 200 + kFalandoMaxMs - 1));
  TEST_ASSERT_TRUE(maquina_tick(m, 200 + kFalandoMaxMs));
  TEST_ASSERT_EQUAL(E_PET, m.st);
}
void test_maquina_ignora_evento_fora_de_hora() {
  Maquina m;
  maquina_init(m, 0);
  TEST_ASSERT_FALSE(maquina_evento(m, EV_OK_SOLTA, 10));
  TEST_ASSERT_FALSE(maquina_evento(m, EV_RESPOSTA, 10));
  TEST_ASSERT_FALSE(maquina_evento(m, EV_VOLTA, 10));
  TEST_ASSERT_EQUAL(E_PET, m.st);
  TEST_ASSERT_FALSE(maquina_tick(m, 999999));  // PET nao estoura nada
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dias_civis_ancoras);
  RUN_TEST(test_epoch_min_consistente);
  RUN_TEST(test_humor_limiar_de_24h);
  RUN_TEST(test_humor_limiar_de_3_dias);
  RUN_TEST(test_humor_nunca_conversou_e_neutro);
  RUN_TEST(test_humor_janela_de_sono);
  RUN_TEST(test_maquina_caminho_feliz);
  RUN_TEST(test_maquina_falha_vai_pra_falando);
  RUN_TEST(test_maquina_estoura_15s_ouvindo);
  RUN_TEST(test_maquina_estoura_60s_falando);
  RUN_TEST(test_maquina_ignora_evento_fora_de_hora);
  return UNITY_END();
}
