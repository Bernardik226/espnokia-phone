#include "snakegame.h"

static uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }

static bool oposta(SnakeDir a, SnakeDir b) {
  return (a == DIR_UP && b == DIR_DOWN) || (a == DIR_DOWN && b == DIR_UP) ||
         (a == DIR_LEFT && b == DIR_RIGHT) || (a == DIR_RIGHT && b == DIR_LEFT);
}

void snake_clear_walls(SnakeGame& g) {
  for (uint16_t i = 0; i < SNAKE_WALL_BYTES; i++) g.walls[i] = 0;
}
void snake_set_wall(SnakeGame& g, uint8_t cols, uint8_t x, uint8_t y) {
  uint16_t c = (uint16_t)y * cols + x;
  if (c < SNAKE_MAX) g.walls[c >> 3] |= (uint8_t)(1u << (c & 7));
}
bool snake_is_wall(const SnakeGame& g, uint8_t cols, uint8_t x, uint8_t y) {
  uint16_t c = (uint16_t)y * cols + x;
  if (c >= SNAKE_MAX) return false;
  return (g.walls[c >> 3] >> (c & 7)) & 1u;
}

void snake_place_food(SnakeGame& g, uint8_t cols, uint8_t rows) {
  for (;;) {
    uint8_t fx = (uint8_t)((lcg(g.rng) >> 16) % cols);
    uint8_t fy = (uint8_t)((lcg(g.rng) >> 16) % rows);
    if (snake_is_wall(g, cols, fx, fy)) continue;   // nao cai em parede
    bool no_corpo = false;
    for (uint16_t i = 0; i < g.len; i++)
      if (g.bx[i] == fx && g.by[i] == fy) { no_corpo = true; break; }
    if (!no_corpo) { g.fx = fx; g.fy = fy; return; }
  }
}

void snake_init(SnakeGame& g, uint8_t cols, uint8_t rows, uint32_t seed) {
  g.len = 3; g.dir = DIR_RIGHT; g.next = DIR_RIGHT;
  g.score = 0; g.alive = true; g.wrap = true; g.rng = seed ? seed : 1;
  snake_clear_walls(g);
  uint8_t cx = cols / 2, cy = rows / 2;
  g.bx[0] = cx;     g.by[0] = cy;
  g.bx[1] = cx - 1; g.by[1] = cy;
  g.bx[2] = cx - 2; g.by[2] = cy;
  snake_place_food(g, cols, rows);
}

void snake_set_dir(SnakeGame& g, SnakeDir d) {
  if (!oposta(g.dir, d)) g.next = d;
}

SnakeEvent snake_step(SnakeGame& g, uint8_t cols, uint8_t rows) {
  g.dir = g.next;
  int nx = g.bx[0], ny = g.by[0];
  if (g.dir == DIR_UP) ny--;
  else if (g.dir == DIR_DOWN) ny++;
  else if (g.dir == DIR_LEFT) nx--;
  else nx++;
  if (g.wrap) {
    nx = (nx + cols) % cols;            // atravessa as bordas
    ny = (ny + rows) % rows;
  } else if (nx < 0 || nx >= cols || ny < 0 || ny >= rows) {
    g.alive = false; return SNK_DIE;    // Snake II: bateu na borda
  }
  if (snake_is_wall(g, cols, (uint8_t)nx, (uint8_t)ny)) {
    g.alive = false; return SNK_DIE;    // bateu numa parede do labirinto
  }
  bool eat = ((uint8_t)nx == g.fx && (uint8_t)ny == g.fy);
  // se nao for comer, a cauda libera sua celula -> nao conta como colisao
  uint16_t check = eat ? g.len : (g.len ? g.len - 1 : 0);
  for (uint16_t i = 0; i < check; i++)
    if (g.bx[i] == (uint8_t)nx && g.by[i] == (uint8_t)ny) {
      g.alive = false; return SNK_DIE;
    }
  uint16_t newlen = eat ? (g.len < SNAKE_MAX ? g.len + 1 : g.len) : g.len;
  for (uint16_t i = newlen - 1; i > 0; i--) { g.bx[i] = g.bx[i - 1]; g.by[i] = g.by[i - 1]; }
  g.bx[0] = (uint8_t)nx; g.by[0] = (uint8_t)ny;
  g.len = newlen;
  if (eat) { g.score++; snake_place_food(g, cols, rows); return SNK_EAT; }
  return SNK_MOVE;
}

uint16_t snake_level_interval_ms(uint8_t level) {
  if (level < 1) level = 1;
  if (level > 9) level = 9;
  return (uint16_t)(260 - (level - 1) * 25);  // 1->260ms (lento) ... 9->60ms (rapido)
}
