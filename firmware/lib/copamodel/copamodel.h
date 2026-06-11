#pragma once
#include <stddef.h>
#include <stdint.h>

// Modelo do contrato /copa/* do server — parseavel no env native (sem hardware).
struct CopaJogo {
  uint8_t dia, mes, h, m;  // kickoff em hora de Brasilia
  char t1[6], t2[6];       // codigo FIFA; placeholders longos do mata-mata truncam
  char info[16];           // "Grupo C", "Round of 32"...
  int8_t s1, s2;           // -1 = sem placar
  bool live;
};

// Preenche jogos[] a partir do JSON do server; retorna quantos entraram (0 em
// JSON invalido). atualizado_s (opcional) recebe a idade do dado no server.
uint8_t copa_parse(const char* json, CopaJogo* jogos, uint8_t max,
                   uint32_t* atualizado_s);

// Linha de lista: "13/6 BRA x MAR" sem placar, "BRA 2x1 MAR" com placar.
void copa_linha(const CopaJogo& j, char* out, size_t len);
