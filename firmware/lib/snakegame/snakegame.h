#pragma once
#include <stdint.h>
#include <stddef.h>

// Engine pura do Snake (sem Arduino): grid em celulas, cresce ao comer, morre
// ao bater em si. Dois modos de borda: wrap (atravessa) ou Snake II (morre na
// parede) + labirinto opcional de celulas bloqueadas. RNG interno (LCG) deixa
// a comida deterministica em teste. A casca (game_snake) cuida de
// render/input/som/tempo e monta o labirinto de cada nivel.
#define SNAKE_MAX 384   // teto do corpo (>= cols*rows; 27*12=324)
#define SNAKE_WALL_BYTES ((SNAKE_MAX + 7) / 8)   // bitset 1 bit por celula

enum SnakeDir : uint8_t { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };
enum SnakeEvent : uint8_t { SNK_MOVE, SNK_EAT, SNK_DIE };

struct SnakeGame {
  uint8_t bx[SNAKE_MAX], by[SNAKE_MAX];  // corpo; [0] = cabeca
  uint16_t len;
  SnakeDir dir, next;                    // dir aplicada / proxima pendente
  uint8_t fx, fy;                        // comida
  uint16_t score;
  bool alive;
  bool wrap;                             // true = atravessa bordas; false = morre (Snake II)
  uint8_t walls[SNAKE_WALL_BYTES];       // labirinto: celulas bloqueadas
  uint32_t rng;                          // estado do LCG
};

void snake_init(SnakeGame& g, uint8_t cols, uint8_t rows, uint32_t seed);
void snake_set_dir(SnakeGame& g, SnakeDir d);            // ignora a oposta a dir
SnakeEvent snake_step(SnakeGame& g, uint8_t cols, uint8_t rows);
void snake_place_food(SnakeGame& g, uint8_t cols, uint8_t rows);
uint16_t snake_level_interval_ms(uint8_t level);         // 1..9 -> ms de tick

// labirinto (paredes). cell = y*cols + x
void snake_clear_walls(SnakeGame& g);
void snake_set_wall(SnakeGame& g, uint8_t cols, uint8_t x, uint8_t y);
bool snake_is_wall(const SnakeGame& g, uint8_t cols, uint8_t x, uint8_t y);
