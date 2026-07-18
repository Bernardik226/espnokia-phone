#pragma once
#include <stdint.h>

// Engine pura do 2048 (sem Arduino): grade 4x4 de EXPOENTES (0 = vazia, e =
// peça de valor 2^e). move() desliza+funde na direção e, se mexeu, põe uma
// peça nova; can_move() diz se ainda há jogada. RNG interno (LCG) deixa tudo
// determinístico em teste. A casca (game_2048) cuida de render/input/som/
// recorde. Espelha o Snake: lógica pura aqui, glue lá.
enum G2Dir : uint8_t { G2_UP, G2_DOWN, G2_LEFT, G2_RIGHT };

struct Game2048 {
  uint8_t cell[16];   // expoente por célula (linha-maior: y*4+x); 0 = vazia
  uint32_t score;
  uint32_t rng;       // estado do LCG
  bool won;           // alcançou 2048 (expoente 11)
};

void g2048_init(Game2048& g, uint32_t seed);   // zera + 2 peças iniciais
bool g2048_move(Game2048& g, G2Dir d);         // true se algo mexeu (funde+score+peça nova)
void g2048_add_random(Game2048& g);            // põe 2 (90%) ou 4 (10%) numa célula vazia
bool g2048_can_move(const Game2048& g);        // false = fim de jogo
