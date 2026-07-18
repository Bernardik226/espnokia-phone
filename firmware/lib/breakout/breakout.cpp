#include "breakout.h"

static uint32_t rnd(Breakout& g) {
  g.rng = g.rng * 1664525u + 1013904223u;
  return g.rng >> 8;
}

static uint8_t row_points(uint8_t row) { return (BO_ROWS - row) * 10; }  // 40..10

static void poe_bola_na_raquete(Breakout& g) {
  g.bx = g.paddle_x + g.paddle_w / 2;
  g.by = g.h - 4;
  g.vx = 1; g.vy = -1;
}

// gera a fase: um padrão (por semente) + tijolos DUROS conforme a dificuldade
static void gen_level(Breakout& g) {
  uint8_t pat = rnd(g) % 5;
  uint8_t tough_pct = (g.dif - 1) * 12;      // 0,12,24,36,48%
  g.restantes = 0;
  for (uint8_t r = 0; r < BO_ROWS; r++)
    for (uint8_t c = 0; c < BO_COLS; c++) {
      bool on;
      switch (pat) {
        case 0: on = true; break;                            // parede cheia
        case 1: on = ((r + c) & 1) == 0; break;              // xadrez
        case 2: on = (c >= r) && (c < BO_COLS - r); break;   // pirâmide
        case 3: on = (rnd(g) % 5) != 0; break;               // buracos aleatórios
        default: on = (c & 1) == 0; break;                   // colunas
      }
      uint8_t v = on ? ((rnd(g) % 100 < tough_pct) ? 2 : 1) : 0;
      g.bricks[r * BO_COLS + c] = v;
      if (v) g.restantes++;
    }
  if (!g.restantes) {                          // padrão vazio: cai pra parede cheia
    for (uint8_t i = 0; i < BO_BRICKS; i++) g.bricks[i] = 1;
    g.restantes = BO_BRICKS;
  }
}

void bo_init(Breakout& g, uint8_t w, uint8_t h, uint8_t dif, uint32_t seed) {
  g.w = w; g.h = h;
  g.paddle_w = BO_PADDLE_W;
  g.brick_w = w / BO_COLS;
  g.paddle_x = (w - g.paddle_w) / 2;
  g.score = 0; g.vidas = 5; g.paddle_hits = 0; g.launched = false;
  g.dif = dif < 1 ? 1 : (dif > 5 ? 5 : dif);
  g.rng = seed ? seed : 1;
  g.pu_falling = false; g.wide_ticks = 0;
  gen_level(g);
  poe_bola_na_raquete(g);
}

void bo_launch(Breakout& g) {
  if (!g.launched) {
    g.launched = true;
    g.vy = -1;
    g.vx = (g.bx < g.w / 2) ? -1 : 1;
  }
}

// aplica a largura alvo (larga com powerup, normal sem) mantendo o centro
static void ajusta_largura(Breakout& g) {
  uint8_t alvo = g.wide_ticks > 0 ? BO_PADDLE_WIDE : BO_PADDLE_W;
  if (g.paddle_w == alvo) return;
  int centro = g.paddle_x + g.paddle_w / 2;
  g.paddle_w = alvo;
  g.paddle_x = centro - g.paddle_w / 2;
  if (g.paddle_x < 0) g.paddle_x = 0;
  if (g.paddle_x > g.w - g.paddle_w) g.paddle_x = g.w - g.paddle_w;
}

void bo_move_paddle(Breakout& g, int dx) {
  int nx = g.paddle_x + dx;
  if (nx < 0) nx = 0;
  if (nx > g.w - g.paddle_w) nx = g.w - g.paddle_w;
  g.paddle_x = nx;
  if (!g.launched) g.bx = g.paddle_x + g.paddle_w / 2;
}

uint16_t bo_ball_interval(const Breakout& g) {
  int ms = 48 - (g.dif - 1) * 4;               // dificuldade acelera a base
  uint16_t sp = g.paddle_hits / 4;             // e cada 4 rebatidas acelera +
  if (sp > 6) sp = 6;
  ms -= sp * 2;
  return ms < 18 ? 18 : ms;
}

BoEvent bo_step(Breakout& g) {
  if (g.wide_ticks > 0 && --g.wide_ticks == 0) ajusta_largura(g);   // powerup expirou

  if (g.pu_falling) {                          // powerup caindo: desce, apanha ou some
    g.pu_y += 1;
    uint8_t py = g.h - 3;
    if (g.pu_y >= py && g.pu_x >= g.paddle_x && g.pu_x < g.paddle_x + g.paddle_w) {
      g.pu_falling = false;
      g.wide_ticks = 500;
      ajusta_largura(g);
      return BO_POWERUP;
    }
    if (g.pu_y >= g.h) g.pu_falling = false;
  }

  if (!g.launched) { g.bx = g.paddle_x + g.paddle_w / 2; g.by = g.h - 4; return BO_NONE; }

  BoEvent ev = BO_NONE;
  int nx = g.bx + g.vx, ny = g.by + g.vy;
  if (nx <= 0)          { nx = 0;       g.vx = 1;  ev = BO_WALL; }
  else if (nx >= g.w-1) { nx = g.w - 1; g.vx = -1; ev = BO_WALL; }
  g.bx = nx; g.by = ny;

  // tijolos (antes da parede de cima)
  if (g.by >= BO_HEADER && g.by < BO_HEADER + BO_ROWS * BO_BRICK_H) {
    uint8_t col = g.bx / g.brick_w;
    uint8_t row = (g.by - BO_HEADER) / BO_BRICK_H;
    if (col < BO_COLS && row < BO_ROWS) {
      uint8_t k = row * BO_COLS + col;
      if (g.bricks[k]) {
        g.vy = -g.vy;
        if (g.bricks[k] == 2) {                // duro: 1º toque só racha
          g.bricks[k] = 1;
          g.score += 5;
          return BO_BRICK;
        }
        g.bricks[k] = 0; g.restantes--;
        g.score += row_points(row);
        if (!g.pu_falling && rnd(g) % 100 < 14) {   // às vezes solta um powerup
          g.pu_falling = true;
          g.pu_x = col * g.brick_w + g.brick_w / 2;
          g.pu_y = BO_HEADER + row * BO_BRICK_H;
        }
        return (g.restantes == 0) ? BO_WIN : BO_BRICK;
      }
    }
  }
  if (g.by <= BO_HEADER) { g.by = BO_HEADER; g.vy = 1; ev = BO_WALL; }

  // raquete
  uint8_t py = g.h - 3;
  if (g.vy > 0 && g.by >= py && g.by <= py + 1 &&
      g.bx >= g.paddle_x && g.bx < g.paddle_x + g.paddle_w) {
    g.vy = -1;
    int rel = g.bx - g.paddle_x;               // esteira por zona da raquete
    if (rel < g.paddle_w / 3) g.vx = -1;
    else if (rel > (2 * g.paddle_w) / 3) g.vx = 1;
    g.paddle_hits++;
    g.by = py - 1;
    return BO_PADDLE;
  }
  // passou da raquete: perde vida
  if (g.by >= g.h - 1) {
    if (g.vidas > 0) g.vidas--;
    if (g.vidas == 0) return BO_OVER;
    g.launched = false;
    poe_bola_na_raquete(g);
    return BO_LIFE;
  }
  return ev;
}
