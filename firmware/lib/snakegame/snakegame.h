#pragma once
#include <stdint.h>
#include <stddef.h>

// Engine pura do Snake (sem Arduino): grid em celulas, wrap nas bordas,
// cresce ao comer, morre ao bater em si. RNG interno (LCG) deixa a comida
// deterministica em teste. A casca (game_snake) cuida de render/input/som/tempo.
#define SNAKE_MAX 384   // teto do corpo (>= cols*rows; 27*12=324)

enum SnakeDir : uint8_t { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };
enum SnakeEvent : uint8_t { SNK_MOVE, SNK_EAT, SNK_DIE };

struct SnakeGame {
  uint8_t bx[SNAKE_MAX], by[SNAKE_MAX];  // corpo; [0] = cabeca
  uint16_t len;
  SnakeDir dir, next;                    // dir aplicada / proxima pendente
  uint8_t fx, fy;                        // comida
  uint16_t score;
  bool alive;
  uint32_t rng;                          // estado do LCG
};

void snake_init(SnakeGame& g, uint8_t cols, uint8_t rows, uint32_t seed);
void snake_set_dir(SnakeGame& g, SnakeDir d);            // ignora a oposta a dir
SnakeEvent snake_step(SnakeGame& g, uint8_t cols, uint8_t rows);
void snake_place_food(SnakeGame& g, uint8_t cols, uint8_t rows);
uint16_t snake_level_interval_ms(uint8_t level);         // 1..9 -> ms de tick
