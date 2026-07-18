#include <unity.h>
#include "game2048.h"

void setUp() {}
void tearDown() {}

// monta o tabuleiro por expoentes e zera score; rng fixo pra add_random ser
// determinístico (os testes só afirmam células fundidas = nunca vazias, então
// a peça nova aleatória não interfere)
static void set(Game2048& g, const uint8_t v[16]) {
  for (int i = 0; i < 16; i++) g.cell[i] = v[i];
  g.score = 0; g.won = false; g.rng = 7;
}

void test_funde_duas_iguais(void) {
  Game2048 g;
  uint8_t v[16] = {1,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};   // 2,2 na linha 0
  set(g, v);
  TEST_ASSERT_TRUE(g2048_move(g, G2_LEFT));
  TEST_ASSERT_EQUAL_UINT8(2, g.cell[0]);                  // 2+2 = 4 (expoente 2)
  TEST_ASSERT_EQUAL_UINT32(4, g.score);
}

void test_tres_iguais_funde_as_duas_adiantadas(void) {
  Game2048 g;
  uint8_t v[16] = {1,1,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};   // 2,2,2
  set(g, v);
  g2048_move(g, G2_LEFT);
  TEST_ASSERT_EQUAL_UINT8(2, g.cell[0]);                  // as duas à esquerda -> 4
  TEST_ASSERT_EQUAL_UINT8(1, g.cell[1]);                  // o 2 solto desliza
  TEST_ASSERT_EQUAL_UINT32(4, g.score);
}

void test_quatro_iguais_faz_dois_pares(void) {
  Game2048 g;
  uint8_t v[16] = {1,1,1,1, 0,0,0,0, 0,0,0,0, 0,0,0,0};   // 2,2,2,2
  set(g, v);
  g2048_move(g, G2_LEFT);
  TEST_ASSERT_EQUAL_UINT8(2, g.cell[0]);                  // 4
  TEST_ASSERT_EQUAL_UINT8(2, g.cell[1]);                  // 4
  TEST_ASSERT_EQUAL_UINT32(8, g.score);                   // dois merges de 4
}

void test_peca_fundida_nao_funde_de_novo(void) {
  Game2048 g;
  uint8_t v[16] = {1,1,2,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};   // 2,2,4
  set(g, v);
  g2048_move(g, G2_LEFT);
  TEST_ASSERT_EQUAL_UINT8(2, g.cell[0]);                  // 2+2 = 4
  TEST_ASSERT_EQUAL_UINT8(2, g.cell[1]);                  // o 4 original NÃO funde
  TEST_ASSERT_EQUAL_UINT32(4, g.score);                   // só um merge
}

void test_movimento_sem_mudanca_retorna_falso(void) {
  Game2048 g;
  uint8_t v[16] = {1,2,3,4, 2,3,4,5, 3,4,5,6, 4,5,6,7};   // cheio, sem par
  set(g, v);
  TEST_ASSERT_FALSE(g2048_move(g, G2_LEFT));
  TEST_ASSERT_EQUAL_UINT8(1, g.cell[0]);                  // nada mudou
}

void test_fim_de_jogo_tabuleiro_travado(void) {
  Game2048 g;
  uint8_t v[16] = {1,2,1,2, 2,1,2,1, 1,2,1,2, 2,1,2,1};   // xadrez: sem vazia nem par
  set(g, v);
  TEST_ASSERT_FALSE(g2048_can_move(g));
}

void test_init_poe_duas_pecas(void) {
  Game2048 g;
  g2048_init(g, 12345);
  int nz = 0;
  for (int i = 0; i < 16; i++) if (g.cell[i]) nz++;
  TEST_ASSERT_EQUAL_INT(2, nz);
  TEST_ASSERT_TRUE(g2048_can_move(g));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_funde_duas_iguais);
  RUN_TEST(test_tres_iguais_funde_as_duas_adiantadas);
  RUN_TEST(test_quatro_iguais_faz_dois_pares);
  RUN_TEST(test_peca_fundida_nao_funde_de_novo);
  RUN_TEST(test_movimento_sem_mudanca_retorna_falso);
  RUN_TEST(test_fim_de_jogo_tabuleiro_travado);
  RUN_TEST(test_init_poe_duas_pecas);
  return UNITY_END();
}
