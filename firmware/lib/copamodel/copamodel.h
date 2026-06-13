#pragma once
#include <stddef.h>
#include <stdint.h>

// Modelo do contrato /copa/* e /futebol/* do server — mesmo shape, o
// aparelho parseia os dois apps com este codigo (env native, sem hardware).
struct CopaJogo {
  uint8_t dia, mes, h, m;  // kickoff em hora de Brasilia
  char t1[6], t2[6];       // codigo FIFA; placeholders longos do mata-mata truncam
  char info[16];           // "Grupo C", "Brasileirão"...
  int8_t s1, s2;           // -1 = sem placar
  bool live;
  char min[6];             // minuto de jogo ("67"); "" fora do ao vivo
  char est[48];            // "SoFi Stadium · Los Angeles"; "" se a fonte nao tem
  char g1[80], g2[80];     // autores de gol, um por linha; "" sem gols
  char n1[20], n2[20];     // nome completo do clube ("Flamengo"); "" na copa
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

// Liga do cardapio dinamico de /futebol/ligas.
struct FutLiga {
  char id[24];  // "bra.1" — vai na query de /futebol/jogos e /futebol/live
  char n[20];   // "Brasileirão" — nome de exibicao (vem pronto do server)
  bool live;    // tem jogo rolando agora
  uint8_t dia, mes;  // ultimo jogo de liga "que passou" (0 = tem jogo por vir)
};

// Preenche ligas[] a partir do JSON de /futebol/ligas; retorna quantas entraram.
uint8_t fut_parse_ligas(const char* json, FutLiga* ligas, uint8_t max);

// Classificacao da liga (/futebol/tabela). O server normaliza em blocos no
// mesmo shape c/p/j/s do /copa/grupos: 1 bloco sem nome = pontos corridos
// (tabela unica numerada); varios blocos "A".."H" = fase de grupos. As linhas
// vivem todas em times[], com gini/gn marcando a fatia de cada bloco.
#define FUT_CLASS_MAX 36  // Libertadores = 8x4=32; Champions League Phase = 36
#define FUT_GRUPO_MAX 8   // teto de blocos (8 grupos)

struct FutClass {
  CopaGrupoTime times[FUT_CLASS_MAX];  // todas as linhas, na ordem do server
  uint8_t nt;                          // total de linhas
  char gnome[FUT_GRUPO_MAX][4];        // "A".."H"; "" na tabela unica
  uint8_t gini[FUT_GRUPO_MAX];         // indice da 1a linha de cada bloco
  uint8_t gn[FUT_GRUPO_MAX];           // quantas linhas tem cada bloco
  uint8_t ng;                          // nº de blocos (1 = tabela unica)
};

// Preenche c a partir do JSON de /futebol/tabela; retorna o nº de blocos
// (0 em JSON invalido ou sem classificacao — mata-mata puro).
uint8_t fut_parse_tabela(const char* json, FutClass* c);

// Linha de lista: "13/6 BRA x MAR" sem placar, "BRA 2x1 MAR" com placar.
void copa_linha(const CopaJogo& j, char* out, size_t len);

// Ultima linha de "A 9'\nB 67'" — o gol mais recente do time ("" se nenhum).
const char* copa_ultimo_gol(const char* gols);
