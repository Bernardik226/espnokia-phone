#include <unity.h>
#include "breakout.h"

void setUp() {}
void tearDown() {}

// posiciona a bola direto (campo 84x48, raquete centrada em x=35..48)
static void bola(Breakout& g, int x, int y, int vx, int vy) {
  g.bx = x; g.by = y; g.vx = vx; g.vy = vy;
}

void test_lanca_a_bola(void) {
  Breakout g; bo_init(g, 84, 48);
  TEST_ASSERT_FALSE(g.launched);
  bo_launch(g);
  TEST_ASSERT_TRUE(g.launched);
  TEST_ASSERT_EQUAL_INT8(-1, g.vy);          // sobe ao lançar
}

void test_quebra_tijolo_e_pontua(void) {
  Breakout g; bo_init(g, 84, 48); bo_launch(g);
  bola(g, 6, 13, 1, -1);                      // subindo pra fileira 1, coluna 0
  BoEvent ev = bo_step(g);
  TEST_ASSERT_EQUAL(BO_BRICK, ev);
  TEST_ASSERT_EQUAL_UINT8(0, g.bricks[1 * BO_COLS + 0]);   // tijolo sumiu
  TEST_ASSERT_EQUAL_UINT8(BO_BRICKS - 1, g.restantes);
  TEST_ASSERT_EQUAL_UINT16(30, g.score);      // fileira 1 vale 30
  TEST_ASSERT_EQUAL_INT8(1, g.vy);            // rebate pra baixo
}

void test_rebate_na_parede_lateral(void) {
  Breakout g; bo_init(g, 84, 48); bo_launch(g);
  bola(g, 1, 30, -1, -1);                     // indo pra esquerda
  BoEvent ev = bo_step(g);
  TEST_ASSERT_EQUAL(BO_WALL, ev);
  TEST_ASSERT_EQUAL_INT8(1, g.vx);            // inverteu
}

void test_raquete_rebate_e_conta_hit(void) {
  Breakout g; bo_init(g, 84, 48); bo_launch(g);
  bola(g, 40, 44, 1, 1);                      // descendo no meio da raquete (35..49)
  BoEvent ev = bo_step(g);
  TEST_ASSERT_EQUAL(BO_PADDLE, ev);
  TEST_ASSERT_EQUAL_INT8(-1, g.vy);           // sobe de volta
  TEST_ASSERT_EQUAL_UINT16(1, g.paddle_hits);
}

void test_perde_vida_ao_passar(void) {
  Breakout g; bo_init(g, 84, 48); bo_launch(g);
  bola(g, 2, 46, -1, 1);                      // longe da raquete, no fundo
  BoEvent ev = bo_step(g);
  TEST_ASSERT_EQUAL(BO_LIFE, ev);
  TEST_ASSERT_EQUAL_UINT8(4, g.vidas);
  TEST_ASSERT_FALSE(g.launched);              // volta pra raquete
}

void test_game_over_na_ultima_vida(void) {
  Breakout g; bo_init(g, 84, 48); bo_launch(g);
  g.vidas = 1;
  bola(g, 2, 46, -1, 1);
  TEST_ASSERT_EQUAL(BO_OVER, bo_step(g));
  TEST_ASSERT_EQUAL_UINT8(0, g.vidas);
}

void test_vitoria_no_ultimo_tijolo(void) {
  Breakout g; bo_init(g, 84, 48); bo_launch(g);
  for (int i = 0; i < BO_BRICKS; i++) g.bricks[i] = 0;
  g.bricks[1 * BO_COLS + 0] = 1; g.restantes = 1;
  bola(g, 6, 13, 1, -1);
  TEST_ASSERT_EQUAL(BO_WIN, bo_step(g));
  TEST_ASSERT_EQUAL_UINT8(0, g.restantes);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_lanca_a_bola);
  RUN_TEST(test_quebra_tijolo_e_pontua);
  RUN_TEST(test_rebate_na_parede_lateral);
  RUN_TEST(test_raquete_rebate_e_conta_hit);
  RUN_TEST(test_perde_vida_ao_passar);
  RUN_TEST(test_game_over_na_ultima_vida);
  RUN_TEST(test_vitoria_no_ultimo_tijolo);
  return UNITY_END();
}
