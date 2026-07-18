#pragma once
#include <stdint.h>

// Engine puro do Breakout (sem Arduino). Bola em px, ±1 por passo (45°); a
// raquete embaixo rebate e ESTEIRA a bola (bater na terça esquerda manda pra
// esquerda, na direita pra direita, no meio segue reto). Tijolos em fileiras:
// as de cima valem mais; alguns são DUROS (2 toques). A fase é gerada por
// dificuldade (1..5) + semente — padrão e densidade variam, e o duro aparece
// mais no difícil. Às vezes um tijolo solta um POWERUP (raquete larga) que cai
// e a raquete apanha. 5 vidas; perde ao deixar a bola passar. A casca cuida de
// render/input/som/tempo (a velocidade da bola vem de bo_ball_interval).
#define BO_COLS 7
#define BO_ROWS 4
#define BO_BRICKS (BO_COLS * BO_ROWS)
#define BO_BRICK_H 4
#define BO_HEADER 8            // topo reservado ao placar; parede de cima aqui
#define BO_PADDLE_W 13
#define BO_PADDLE_WIDE 21      // raquete com o powerup

enum BoEvent : uint8_t {
  BO_NONE, BO_WALL, BO_BRICK, BO_PADDLE, BO_POWERUP, BO_LIFE, BO_OVER, BO_WIN
};

struct Breakout {
  int16_t bx, by;
  int8_t vx, vy;
  int16_t paddle_x;
  uint8_t w, h, paddle_w, brick_w;
  uint8_t bricks[BO_BRICKS];   // 0=vazio, 1=normal, 2=duro (2 toques)
  uint8_t restantes;           // tijolos que ainda faltam destruir
  uint16_t score;
  uint8_t vidas;
  uint16_t paddle_hits;
  bool launched;
  uint8_t dif;                 // 1..5 (dificuldade escolhida no menu)
  uint32_t rng;
  bool pu_falling;             // powerup caindo?
  int16_t pu_x, pu_y;
  uint16_t wide_ticks;         // >0 = raquete larga por N passos
};

void bo_init(Breakout& g, uint8_t w, uint8_t h, uint8_t dif, uint32_t seed);
void bo_launch(Breakout& g);
void bo_move_paddle(Breakout& g, int dx);   // move e clampa a raquete
BoEvent bo_step(Breakout& g);               // 1 passo de física
uint16_t bo_ball_interval(const Breakout& g);  // ms/passo (mais dif/rebatidas = + rápido)
