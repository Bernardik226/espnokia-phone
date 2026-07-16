#include "jogo_view.h"
#include <stdio.h>
#include <string.h>
#include "assets.h"
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
  nokia_ui::text_center(g, y, linha);
}

// proxima linha de uma lista "A 9'\nB 67'": copia pra out e avanca o ponteiro
// (out vazio quando a lista acabou)
static const char* pega_linha(const char* p, char* out, size_t cap) {
  out[0] = '\0';
  if (!*p) return p;
  const char* nl = strchr(p, '\n');
  size_t len = nl ? (size_t)(nl - p) : strlen(p);
  if (len >= cap) len = cap - 1;
  memcpy(out, p, len);
  out[len] = '\0';
  return nl ? nl + 1 : p + strlen(p);
}

// poda um autor de gol preservando o minuto, que e o que situa o lance:
// "Richarlison 90+2'" → "Richarl. 90+2'" (o corte come so o nome)
static void poda_gol(U8G2& g, char* s, size_t cap, int max_w) {
  if ((int)g.getUTF8Width(s) <= max_w) return;
  char suf[10] = "";
  char* sp = strrchr(s, ' ');
  if (sp && strlen(sp) < sizeof(suf)) {  // separa o " 90+2'" do fim
    snprintf(suf, sizeof(suf), "%s", sp);
    *sp = '\0';
  }
  nokia_ui::poda(g, s, max_w - (int)g.getUTF8Width(suf));
  size_t n = strlen(s);
  snprintf(s + n, cap - n, "%s", suf);
}

// autores de gol da ultima pagina: t1 a esquerda, t2 a direita, linha a
// linha. Quando os dois lados tem gol na mesma linha, cada um fica na sua
// metade (poda no nome) — autor comprido nao invade mais o lado do outro;
// lado sozinho na linha usa a tela inteira.
static void desenha_gols(U8G2& g, const char* g1, const char* g2) {
  const char* p1 = g1;
  const char* p2 = g2;
  for (int y = 16; y <= 30 && (*p1 || *p2); y += 7) {
    char l1[28], l2[28];
    p1 = pega_linha(p1, l1, sizeof(l1));
    p2 = pega_linha(p2, l2, sizeof(l2));
    if (l1[0]) {
      poda_gol(g, l1, sizeof(l1), l2[0] ? 38 : 80);
      g.drawUTF8(2, y, l1);
    }
    if (l2[0]) {
      poda_gol(g, l2, sizeof(l2), l1[0] ? 38 : 80);
      g.drawUTF8(82 - (int)g.getUTF8Width(l2), y, l2);
    }
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
    desenha_gols(g, j.g1, j.g2);
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
  nokia_ui::text_center(g, 30, j.info);
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
    nokia_ui::text_center(g, 39, tr(STR_NOTIFY_ON));
  } else if (j.s1 >= 0 && j.s2 >= 0) {
    // jogo encerrado: quando foi, com o reloginho de historico
    snprintf(l2, sizeof(l2), "%02u/%02u %02u:%02u", j.dia, j.mes, j.h, j.m);
    int x = 42 - ((int)g.getStrWidth(l2) - (MINI_CLOCK_W + 2)) / 2;
    g.drawXBMP(x - MINI_CLOCK_W - 2, 33, MINI_CLOCK_W, MINI_CLOCK_H,
               mini_clock_bits);
    g.drawStr(x, 39, l2);
  }
  nokia_ui::softkey(g, tr(tem_aviso ? STR_NOTIFY_OFF : STR_NOTIFY));
  g.drawStr(79, 47, "v");  // tem pagina pra baixo
}
