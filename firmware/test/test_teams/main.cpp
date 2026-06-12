#include <unity.h>
#include <string.h>
#include "i18n.h"
#include "teams.h"

void test_nome_segue_o_idioma_do_sistema() {
  i18n_set(LANG_PT);
  TEST_ASSERT_EQUAL_STRING("Brasil", team_name("BRA"));
  TEST_ASSERT_EQUAL_STRING("Estados Unidos", team_name("USA"));
  i18n_set(LANG_DE);
  TEST_ASSERT_EQUAL_STRING("Deutschland", team_name("GER"));
  i18n_set(LANG_FI);
  TEST_ASSERT_EQUAL_STRING("Etelä-Korea", team_name("KOR"));
  i18n_set(LANG_PT);  // nao vaza idioma pros outros testes
}

void test_codigo_desconhecido_volta_o_codigo() {
  // placeholder do mata-mata truncado em t1[6] cai aqui
  TEST_ASSERT_EQUAL_STRING("Runne", team_name("Runne"));
  TEST_ASSERT_EQUAL_STRING("", team_name(""));
}

void test_tabela_tem_as_48_selecoes() {
  TEST_ASSERT_EQUAL_UINT8(48, team_count());
}

void test_nenhum_nome_estoura_a_tela() {
  // ~16 colunas na fonte small: nomes maiores quebram o layout do detail
  static const char* kCodes[] = {
      "ALG", "ARG", "AUS", "AUT", "BEL", "BIH", "BRA", "CAN", "CIV", "COD",
      "COL", "CPV", "CRO", "CUW", "CZE", "ECU", "EGY", "ENG", "ESP", "FRA",
      "GER", "GHA", "HAI", "IRN", "IRQ", "JOR", "JPN", "KOR", "KSA", "MAR",
      "MEX", "NED", "NOR", "NZL", "PAN", "PAR", "POR", "QAT", "RSA", "SCO",
      "SEN", "SUI", "SWE", "TUN", "TUR", "URU", "USA", "UZB"};
  for (uint8_t l = 0; l < LANG_COUNT; l++) {
    i18n_set((Lang)l);
    for (const char* c : kCodes) {
      const char* nome = team_name(c);
      // fora da tabela voltaria o MESMO ponteiro; strings podem coincidir
      // de proposito (DE chama os EUA de "USA")
      TEST_ASSERT_TRUE_MESSAGE(nome != c, c);
      // colunas visiveis: bytes UTF-8 de continuacao (0x80..0xBF) nao contam
      uint8_t cols = 0;
      for (const char* p = nome; *p; p++)
        if (((uint8_t)*p & 0xC0) != 0x80) cols++;
      TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(16, cols, nome);
    }
  }
  i18n_set(LANG_PT);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_nome_segue_o_idioma_do_sistema);
  RUN_TEST(test_codigo_desconhecido_volta_o_codigo);
  RUN_TEST(test_tabela_tem_as_48_selecoes);
  RUN_TEST(test_nenhum_nome_estoura_a_tela);
  return UNITY_END();
}
