#include "jogo_view.h"
#include <stdio.h>
#include <string.h>
#include "i18n.h"
#include "nokia_ui.h"
#include "teams.h"

// pagina de equipes: "BRA - Brasil" / "FLA - Flamengo" centrado. O nome vem
// do server (n1/n2, clube na lingua da liga); selecao sem n1 cai pra tabela
// local traduzida. Clube sem nome em lugar nenhum fica so no codigo. Se o
// nome completo nao deixa caber o prefixo na tela, mostra so o nome.
static void linha_equipe(U8G2& g, const char* code, const char* nome, int y) {
  if (!nome[0]) nome = team_name(code);
  char linha[28];
  if (strcmp(nome, code) == 0)
    snprintf(linha, sizeof(linha), "%s", code);
  else
    snprintf(linha, sizeof(linha), "%s - %s", code, nome);
  if ((int)g.getUTF8Width(linha) > 80)
    snprintf(linha, sizeof(linha), "%s", nome);
  g.drawUTF8(42 - (int)g.getUTF8Width(linha) / 2, y, linha);
}

// autores de gol da ultima pagina: um por linha, t1 a esquerda e t2 a direita
static void desenha_gols(U8G2& g, const char* gols, bool direita) {
  int y = 16;
  const char* p = gols;
  while (*p && y <= 30) {
    const char* nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    char linha[28];
    if (len >= sizeof(linha)) len = sizeof(linha) - 1;
    memcpy(linha, p, len);
    linha[len] = '\0';
    int x = direita ? 82 - (int)g.getUTF8Width(linha) : 2;
    g.drawUTF8(x, y, linha);
    y += 7;
    if (!nl) break;
    p = nl + 1;
  }
}

void jogo_view_detail(U8G2& g, const CopaJogo& j, uint8_t pag,
                      bool tem_aviso) {
  char l1[16];
  snprintf(l1, sizeof(l1), "%s x %s", j.t1, j.t2);
  nokia_ui::text_bold_center(g, 8, l1);
  if (pag == 1) {  // pagina 2: nome completo das equipes
    linha_equipe(g, j.t1, j.n1, 21);
    g.drawStr(40, 29, "x");
    linha_equipe(g, j.t2, j.n2, 37);
    g.drawStr(73, 47, "^");
    g.drawStr(79, 47, "v");
    return;
  }
  if (pag == 2) {  // pagina 3: autores dos gols + estadio
    desenha_gols(g, j.g1, false);
    desenha_gols(g, j.g2, true);
    if (j.est[0]) {
      const char* sep = strstr(j.est, " \xc2\xb7 ");  // " · " do server
      if (sep) {
        char nome[40];
        size_t len = (size_t)(sep - j.est);
        if (len >= sizeof(nome)) len = sizeof(nome) - 1;
        memcpy(nome, j.est, len);
        nome[len] = '\0';
        g.drawUTF8(2, 38, nome);
        g.drawUTF8(2, 45, sep + 4);
      } else {
        g.drawUTF8(2, 45, j.est);
      }
    }
    g.drawStr(79, 47, "^");  // tem pagina pra cima
    return;
  }
  char l2[24];
  if (j.s1 >= 0 && j.s2 >= 0) {
    snprintf(l2, sizeof(l2), "%d x %d", j.s1, j.s2);
    nokia_ui::text_bold_center(g, 20, l2);
  } else {
    snprintf(l2, sizeof(l2), "%02u/%02u %02u:%02u", j.dia, j.mes, j.h, j.m);
    g.drawStr(42 - (int)g.getStrWidth(l2) / 2, 20, l2);
  }
  g.drawUTF8(42 - (int)g.getUTF8Width(j.info) / 2, 30, j.info);
  if (j.live) {
    if (j.min[0]) {  // minuto de jogo ao lado: "AO VIVO 67'"
      bool num = strspn(j.min, "0123456789") == strlen(j.min);
      snprintf(l2, sizeof(l2), "%s %s%s", tr(STR_LIVE_BIG), j.min,
               num ? "'" : "");
      nokia_ui::text_bold_center(g, 39, l2);
    } else {
      nokia_ui::text_bold_center(g, 39, tr(STR_LIVE_BIG));
    }
  } else if (tem_aviso) {
    g.drawUTF8(42 - (int)g.getUTF8Width(tr(STR_NOTIFY_ON)) / 2, 39,
               tr(STR_NOTIFY_ON));
  }
  nokia_ui::softkey(g, tr(tem_aviso ? STR_NOTIFY_OFF : STR_NOTIFY));
  g.drawStr(79, 47, "v");  // tem pagina pra baixo
}
