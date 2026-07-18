#include "breakout.h"

static uint8_t row_points(uint8_t row) {  // fileira de cima (row 0) vale mais
  return (BO_ROWS - row) * 10;            // 40, 30, 20, 10
}

static void poe_bola_na_raquete(Breakout& g) {
  g.bx = g.paddle_x + g.paddle_w / 2;
  g.by = g.h - 4;                          // logo acima da raquete
  g.vx = 1; g.vy = -1;
}

void bo_init(Breakout& g, uint8_t w, uint8_t h) {
  g.w = w; g.h = h;
  g.paddle_w = BO_PADDLE_W;
  g.brick_w = w / BO_COLS;
  g.paddle_x = (w - g.paddle_w) / 2;
  for (uint8_t i = 0; i < BO_BRICKS; i++) g.bricks[i] = 1;
  g.restantes = BO_BRICKS;
  g.score = 0; g.vidas = 5; g.paddle_hits = 0;
  g.launched = false;
  poe_bola_na_raquete(g);
}

void bo_launch(Breakout& g) {
  if (!g.launched) {
    g.launched = true;
    g.vy = -1;
    g.vx = (g.bx < g.w / 2) ? -1 : 1;
  }
}

void bo_move_paddle(Breakout& g, int dx) {
  int nx = g.paddle_x + dx;
  if (nx < 0) nx = 0;
  if (nx > g.w - g.paddle_w) nx = g.w - g.paddle_w;
  g.paddle_x = nx;
  if (!g.launched) g.bx = g.paddle_x + g.paddle_w / 2;   // bola acompanha
}

BoEvent bo_step(Breakout& g) {
  if (!g.launched) { g.bx = g.paddle_x + g.paddle_w / 2; g.by = g.h - 4; return BO_NONE; }

  BoEvent ev = BO_NONE;
  int nx = g.bx + g.vx, ny = g.by + g.vy;
  // paredes laterais
  if (nx <= 0)          { nx = 0;        g.vx = 1;  ev = BO_WALL; }
  else if (nx >= g.w-1) { nx = g.w - 1;  g.vx = -1; ev = BO_WALL; }
  g.bx = nx; g.by = ny;

  // tijolos (antes da parede de cima: um tijolo na 1a fileira rebate, não a parede)
  if (g.by >= BO_HEADER && g.by < BO_HEADER + BO_ROWS * BO_BRICK_H) {
    uint8_t col = g.bx / g.brick_w;
    uint8_t row = (g.by - BO_HEADER) / BO_BRICK_H;
    if (col < BO_COLS && row < BO_ROWS) {
      uint8_t k = row * BO_COLS + col;
      if (g.bricks[k]) {
        g.bricks[k] = 0; g.restantes--;
        g.score += row_points(row);
        g.vy = -g.vy;
        return (g.restantes == 0) ? BO_WIN : BO_BRICK;
      }
    }
  }
  // parede de cima (base do placar)
  if (g.by <= BO_HEADER) { g.by = BO_HEADER; g.vy = 1; ev = BO_WALL; }

  // raquete (linha h-3): descendo e dentro do range em x
  uint8_t py = g.h - 3;
  if (g.vy > 0 && g.by >= py && g.by <= py + 1 &&
      g.bx >= g.paddle_x && g.bx < g.paddle_x + g.paddle_w) {
    g.vy = -1;
    int rel = g.bx - g.paddle_x;                 // esteira por zona da raquete
    if (rel < g.paddle_w / 3) g.vx = -1;
    else if (rel > (2 * g.paddle_w) / 3) g.vx = 1;
    g.paddle_hits++;
    g.by = py - 1;
    return BO_PADDLE;
  }
  // passou da raquete: perde vida (ou acaba)
  if (g.by >= g.h - 1) {
    if (g.vidas > 0) g.vidas--;
    if (g.vidas == 0) return BO_OVER;
    g.launched = false;
    poe_bola_na_raquete(g);
    return BO_LIFE;
  }
  return ev;
}
