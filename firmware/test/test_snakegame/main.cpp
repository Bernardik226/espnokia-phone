#include <unity.h>
#include "snakegame.h"

void test_init_estado_inicial() {
  SnakeGame g; snake_init(g, 20, 10, 1);
  TEST_ASSERT_EQUAL_UINT16(3, g.len);
  TEST_ASSERT_TRUE(g.alive);
  TEST_ASSERT_EQUAL_UINT8(DIR_RIGHT, g.dir);
  TEST_ASSERT_EQUAL_UINT16(0, g.score);
  TEST_ASSERT_EQUAL_UINT8(10, g.bx[0]);  // centro = cols/2
  TEST_ASSERT_EQUAL_UINT8(5, g.by[0]);   // centro = rows/2
}

void test_step_move_na_direcao() {
  SnakeGame g; snake_init(g, 20, 10, 1);
  uint8_t x0 = g.bx[0];
  TEST_ASSERT_EQUAL_UINT8(SNK_MOVE, snake_step(g, 20, 10));
  TEST_ASSERT_EQUAL_UINT8(x0 + 1, g.bx[0]);   // andou pra direita
  TEST_ASSERT_EQUAL_UINT16(3, g.len);         // sem comer, nao cresce
}

void test_set_dir_bloqueia_re_180() {
  SnakeGame g; snake_init(g, 20, 10, 1);      // dir = RIGHT
  snake_set_dir(g, DIR_LEFT);                 // oposta -> ignorada
  snake_step(g, 20, 10);
  TEST_ASSERT_EQUAL_UINT8(DIR_RIGHT, g.dir);
  snake_set_dir(g, DIR_UP);                   // perpendicular -> aceita
  snake_step(g, 20, 10);
  TEST_ASSERT_EQUAL_UINT8(DIR_UP, g.dir);
}

void test_wrap_nas_quatro_bordas() {
  SnakeGame g;
  // direita: cabeca na ultima coluna -> volta pra 0
  snake_init(g, 10, 10, 1); g.bx[0] = 9; g.dir = g.next = DIR_RIGHT;
  g.fx = 0; g.fy = 9;  // comida longe da cabeca
  snake_step(g, 10, 10); TEST_ASSERT_EQUAL_UINT8(0, g.bx[0]);
  // esquerda
  snake_init(g, 10, 10, 1); g.bx[0] = 0; g.dir = g.next = DIR_LEFT;
  g.fx = 5; g.fy = 9; snake_step(g, 10, 10); TEST_ASSERT_EQUAL_UINT8(9, g.bx[0]);
  // cima
  snake_init(g, 10, 10, 1); g.by[0] = 0; g.dir = g.next = DIR_UP;
  g.fx = 0; g.fy = 9; snake_step(g, 10, 10); TEST_ASSERT_EQUAL_UINT8(9, g.by[0]);
  // baixo
  snake_init(g, 10, 10, 1); g.by[0] = 9; g.dir = g.next = DIR_DOWN;
  g.fx = 0; g.fy = 0; snake_step(g, 10, 10); TEST_ASSERT_EQUAL_UINT8(0, g.by[0]);
}

void test_colisao_consigo_mata() {
  SnakeGame g; snake_init(g, 10, 10, 1);
  g.fx = 0; g.fy = 0;                 // comida longe
  // cobra em U: cabeca (5,5) indo RIGHT bate no segmento (6,5)
  g.len = 5;
  g.bx[0] = 5; g.by[0] = 5;
  g.bx[1] = 5; g.by[1] = 6;
  g.bx[2] = 6; g.by[2] = 6;
  g.bx[3] = 6; g.by[3] = 5;          // <- a cabeca vai cair aqui
  g.bx[4] = 6; g.by[4] = 4;
  g.dir = g.next = DIR_RIGHT;
  TEST_ASSERT_EQUAL_UINT8(SNK_DIE, snake_step(g, 10, 10));
  TEST_ASSERT_FALSE(g.alive);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_init_estado_inicial);
  RUN_TEST(test_step_move_na_direcao);
  RUN_TEST(test_set_dir_bloqueia_re_180);
  RUN_TEST(test_wrap_nas_quatro_bordas);
  RUN_TEST(test_colisao_consigo_mata);
  return UNITY_END();
}
