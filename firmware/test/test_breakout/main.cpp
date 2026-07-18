#include <unity.h>
#include "breakout.h"

void setUp() {}
void tearDown() {}

// campo 84x48, dif 1, semente fixa; raquete centrada
static void novo(Breakout& g) { bo_init(g, 84, 48, 1, 1); }
static void bola(Breakout& g, int x, int y, int vx, int vy) {
  g.bx = x; g.by = y; g.vx = vx; g.vy = vy;
}
static void so_um_tijolo(Breakout& g, uint8_t k, uint8_t v) {
  for (uint8_t i = 0; i < BO_BRICKS; i++) g.bricks[i] = 0;
  g.bricks[k] = v; g.restantes = 1;
}

void test_lanca_a_bola(void) {
  Breakout g; novo(g);
  TEST_ASSERT_FALSE(g.launched);
  bo_launch(g);
  TEST_ASSERT_TRUE(g.launched);
  TEST_ASSERT_EQUAL_INT8(-1, g.vy);
}

void test_quebra_tijolo_e_pontua(void) {
  Breakout g; novo(g); bo_launch(g);
  for (uint8_t i = 0; i < BO_BRICKS; i++) g.bricks[i] = 1;   // parede cheia
  g.restantes = BO_BRICKS;
  bola(g, 6, 13, 1, -1);                                     // fileira 1, coluna 0
  TEST_ASSERT_EQUAL(BO_BRICK, bo_step(g));
  TEST_ASSERT_EQUAL_UINT8(0, g.bricks[1 * BO_COLS + 0]);
  TEST_ASSERT_EQUAL_UINT8(BO_BRICKS - 1, g.restantes);
  TEST_ASSERT_EQUAL_UINT16(30, g.score);                    // fileira 1 = 30
  TEST_ASSERT_EQUAL_INT8(1, g.vy);
}

void test_tijolo_duro_dois_toques(void) {
  Breakout g; novo(g); bo_launch(g);
  so_um_tijolo(g, 1 * BO_COLS + 0, 2);                       // um tijolo DURO
  bola(g, 6, 13, 1, -1);
  TEST_ASSERT_EQUAL(BO_BRICK, bo_step(g));                   // 1º toque: só racha
  TEST_ASSERT_EQUAL_UINT8(1, g.bricks[1 * BO_COLS + 0]);     // virou normal
  TEST_ASSERT_EQUAL_UINT8(1, g.restantes);                  // ainda conta
  bola(g, 6, 13, 1, -1);
  TEST_ASSERT_EQUAL(BO_WIN, bo_step(g));                     // 2º toque destrói (último)
  TEST_ASSERT_EQUAL_UINT8(0, g.restantes);
}

void test_rebate_na_parede_lateral(void) {
  Breakout g; novo(g); bo_launch(g);
  bola(g, 1, 30, -1, -1);
  TEST_ASSERT_EQUAL(BO_WALL, bo_step(g));
  TEST_ASSERT_EQUAL_INT8(1, g.vx);
}

void test_raquete_rebate_e_conta_hit(void) {
  Breakout g; novo(g); bo_launch(g);
  bola(g, 40, 44, 1, 1);
  TEST_ASSERT_EQUAL(BO_PADDLE, bo_step(g));
  TEST_ASSERT_EQUAL_INT8(-1, g.vy);
  TEST_ASSERT_EQUAL_UINT16(1, g.paddle_hits);
}

void test_perde_vida_ao_passar(void) {
  Breakout g; novo(g); bo_launch(g);
  bola(g, 2, 46, -1, 1);
  TEST_ASSERT_EQUAL(BO_LIFE, bo_step(g));
  TEST_ASSERT_EQUAL_UINT8(4, g.vidas);
  TEST_ASSERT_FALSE(g.launched);
}

void test_game_over_na_ultima_vida(void) {
  Breakout g; novo(g); bo_launch(g);
  g.vidas = 1;
  bola(g, 2, 46, -1, 1);
  TEST_ASSERT_EQUAL(BO_OVER, bo_step(g));
  TEST_ASSERT_EQUAL_UINT8(0, g.vidas);
}

void test_vitoria_no_ultimo_tijolo(void) {
  Breakout g; novo(g); bo_launch(g);
  so_um_tijolo(g, 1 * BO_COLS + 0, 1);
  bola(g, 6, 13, 1, -1);
  TEST_ASSERT_EQUAL(BO_WIN, bo_step(g));
  TEST_ASSERT_EQUAL_UINT8(0, g.restantes);
}

void test_powerup_alarga_a_raquete(void) {
  Breakout g; novo(g); bo_launch(g);
  g.pu_falling = true; g.pu_x = 40; g.pu_y = g.h - 3;        // caindo em cima da raquete
  TEST_ASSERT_EQUAL(BO_POWERUP, bo_step(g));
  TEST_ASSERT_EQUAL_UINT8(BO_PADDLE_WIDE, g.paddle_w);
  TEST_ASSERT_TRUE(g.wide_ticks > 0);
}

void test_dificuldade_acelera_a_bola(void) {
  Breakout f, d;
  bo_init(f, 84, 48, 1, 1);
  bo_init(d, 84, 48, 5, 1);
  TEST_ASSERT_TRUE(bo_ball_interval(d) < bo_ball_interval(f));   // difícil = mais rápido
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_lanca_a_bola);
  RUN_TEST(test_quebra_tijolo_e_pontua);
  RUN_TEST(test_tijolo_duro_dois_toques);
  RUN_TEST(test_rebate_na_parede_lateral);
  RUN_TEST(test_raquete_rebate_e_conta_hit);
  RUN_TEST(test_perde_vida_ao_passar);
  RUN_TEST(test_game_over_na_ultima_vida);
  RUN_TEST(test_vitoria_no_ultimo_tijolo);
  RUN_TEST(test_powerup_alarga_a_raquete);
  RUN_TEST(test_dificuldade_acelera_a_bola);
  return UNITY_END();
}
