#pragma once
#include <stdint.h>

// Engine puro do Breakout (sem Arduino). Bola em px, velocidade ±1 por passo
// (45°); a raquete embaixo rebate e ESTEIRA a bola: bater na terça esquerda
// manda pra esquerda, na direita pra direita, no meio segue reto. Tijolos em
// fileiras — as de cima valem mais pontos (40/30/20/10). 5 vidas; perde ao
// deixar a bola passar. paddle_hits acelera (o glue encurta o tick a cada 4).
// A casca (game_breakout) cuida de render/input/som/tempo.
#define BO_COLS 7
#define BO_ROWS 4
#define BO_BRICKS (BO_COLS * BO_ROWS)
#define BO_BRICK_H 4      // altura de cada fileira (px)
#define BO_HEADER 8       // topo reservado ao placar; parede de cima aqui
#define BO_PADDLE_W 14

enum BoEvent : uint8_t {
  BO_NONE, BO_WALL, BO_BRICK, BO_PADDLE, BO_LIFE, BO_OVER, BO_WIN
};

struct Breakout {
  int16_t bx, by;             // bola (px)
  int8_t vx, vy;              // velocidade ±1
  int16_t paddle_x;           // canto esquerdo da raquete (px)
  uint8_t w, h, paddle_w, brick_w;
  uint8_t bricks[BO_BRICKS];  // 1 = existe
  uint8_t restantes;
  uint16_t score;
  uint8_t vidas;
  uint16_t paddle_hits;       // pra acelerar a bola
  bool launched;              // bola presa na raquete até lançar
};

void bo_init(Breakout& g, uint8_t w, uint8_t h);
void bo_launch(Breakout& g);
void bo_move_paddle(Breakout& g, int dx);   // move e clampa a raquete
BoEvent bo_step(Breakout& g);               // 1 passo de física
