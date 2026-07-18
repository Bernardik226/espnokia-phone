#include "game2048.h"

// LCG (mesmo do Snake): determinístico em teste, barato no ESP32
static uint32_t rnd(Game2048& g) {
  g.rng = g.rng * 1664525u + 1013904223u;
  return g.rng >> 8;
}

void g2048_add_random(Game2048& g) {
  uint8_t vazias[16], n = 0;
  for (uint8_t i = 0; i < 16; i++)
    if (!g.cell[i]) vazias[n++] = i;
  if (!n) return;
  uint8_t idx = vazias[rnd(g) % n];
  g.cell[idx] = (rnd(g) % 10 == 0) ? 2 : 1;   // 4 em 10% das vezes, senão 2
}

void g2048_init(Game2048& g, uint32_t seed) {
  for (uint8_t i = 0; i < 16; i++) g.cell[i] = 0;
  g.score = 0;
  g.won = false;
  g.rng = seed ? seed : 1;
  g2048_add_random(g);
  g2048_add_random(g);
}

// desliza uma linha de 4 para o índice 0 (a "esquerda"), fundindo pares iguais
// uma única vez (a peça fundida não funde de novo). Regra do 2048: 3 iguais ->
// as duas mais adiantadas fundem; 4 iguais -> dois pares. Retorna se mexeu.
static bool slide4(uint8_t a[4], uint32_t& score) {
  uint8_t comp[4]; uint8_t c = 0;
  for (uint8_t i = 0; i < 4; i++)
    if (a[i]) comp[c++] = a[i];              // compacta (tira buracos)
  uint8_t out[4] = {0, 0, 0, 0}; uint8_t w = 0;
  for (uint8_t i = 0; i < c; i++) {
    if (i + 1 < c && comp[i] == comp[i + 1]) {
      out[w++] = comp[i] + 1;                // funde: expoente +1
      score += (1u << (comp[i] + 1));        // ganha o valor da peça nova
      i++;                                   // pula a peça consumida
    } else {
      out[w++] = comp[i];
    }
  }
  bool moved = false;
  for (uint8_t i = 0; i < 4; i++) {
    if (a[i] != out[i]) moved = true;
    a[i] = out[i];
  }
  return moved;
}

// índice da célula ao percorrer uma linha/coluna com o índice 0 = destino do
// deslize (assim slide4 sempre "empurra pra esquerda")
static uint8_t idx_of(G2Dir d, uint8_t line, uint8_t i) {
  uint8_t x, y;
  switch (d) {
    case G2_LEFT:  x = i;     y = line;  break;
    case G2_RIGHT: x = 3 - i; y = line;  break;
    case G2_UP:    x = line;  y = i;     break;
    default:       x = line;  y = 3 - i; break;   // G2_DOWN
  }
  return y * 4 + x;
}

bool g2048_move(Game2048& g, G2Dir d) {
  bool moved = false;
  for (uint8_t line = 0; line < 4; line++) {
    uint8_t v[4];
    for (uint8_t i = 0; i < 4; i++) v[i] = g.cell[idx_of(d, line, i)];
    if (slide4(v, g.score)) moved = true;
    for (uint8_t i = 0; i < 4; i++) g.cell[idx_of(d, line, i)] = v[i];
  }
  if (moved) {
    for (uint8_t i = 0; i < 16; i++)
      if (g.cell[i] >= 11) g.won = true;     // 2^11 = 2048
    g2048_add_random(g);
  }
  return moved;
}

bool g2048_can_move(const Game2048& g) {
  for (uint8_t i = 0; i < 16; i++)
    if (!g.cell[i]) return true;             // tem célula vazia
  for (uint8_t y = 0; y < 4; y++)
    for (uint8_t x = 0; x < 4; x++) {
      uint8_t c = g.cell[y * 4 + x];
      if (x < 3 && c == g.cell[y * 4 + x + 1]) return true;   // par horizontal
      if (y < 3 && c == g.cell[(y + 1) * 4 + x]) return true; // par vertical
    }
  return false;
}
