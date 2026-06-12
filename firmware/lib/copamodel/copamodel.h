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
  char min[6];             // minuto de jogo ("67"); "" fora do ao vivo
  char est[48];            // "SoFi Stadium · Los Angeles"; "" se a fonte nao tem
  char g1[80], g2[80];     // autores de gol, um por linha; "" sem gols
};

// Uma linha da classificacao: codigo FIFA, pontos, jogos, saldo de gols.
struct CopaGrupoTime {
  char c[6];
  int8_t pts, j, sg;
};

struct CopaGrupo {
  char nome[4];        // "A".."L"
  CopaGrupoTime t[4];  // ja vem classificado do server
  uint8_t nt;
};

// Preenche jogos[] a partir do JSON do server; retorna quantos entraram (0 em
// JSON invalido). atualizado_s (opcional) recebe a idade do dado no server.
uint8_t copa_parse(const char* json, CopaJogo* jogos, uint8_t max,
                   uint32_t* atualizado_s);

// Preenche gs[] a partir do JSON de /copa/grupos; retorna quantos entraram.
uint8_t copa_parse_grupos(const char* json, CopaGrupo* gs, uint8_t max);

// Linha de lista: "13/6 BRA x MAR" sem placar, "BRA 2x1 MAR" com placar.
void copa_linha(const CopaJogo& j, char* out, size_t len);

// Ultima linha de "A 9'\nB 67'" — o gol mais recente do time ("" se nenhum).
const char* copa_ultimo_gol(const char* gols);
