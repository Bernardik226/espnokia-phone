#include "app_futebol.h"
#include <Arduino.h>
#include <string.h>
#include <U8g2lib.h>
#include "copamodel.h"
#include "i18n.h"
#include "jogo_aviso.h"
#include "net/http.h"
#include "net/wifi.h"
#include "sound.h"
#include "drivers/buzzer.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/goal_fx.h"
#include "ui/jogo_view.h"
#include "ui/nokia_ui.h"

// Futebol de clubes via server companion: mesmo esqueleto do app da Copa
// (lista de jogos → detail em 3 paginas → par de avisos de gol), trocando
// as abas fixas por um cardapio dinamico de ligas (V_LIGAS) que o server
// monta conforme o que esta rolando na temporada — Copa do Brasil e
// Libertadores aparecem sozinhas quando abrem. O vigia de gols e o mesmo da
// Copa, armado com o path da liga ("/futebol/live?liga=X").
//
// Cada liga abre num submenu (V_LIGA: Jogos / Tabela). A V_TABELA se adapta ao
// formato que o server manda em /futebol/tabela: 1 bloco = pontos corridos
// (tabela unica numerada, rolavel); varios = fase de grupos (navegavel, igual
// a Copa). Mata-mata puro nao tem classificacao → "sem tabela".
enum View : uint8_t { V_LIGAS, V_LIGA, V_LIST, V_DETAIL, V_TABELA };
enum Fetch : uint8_t { FETCH_IDLE, FETCH_PENDING, FETCH_OK, FETCH_ERR,
                       FETCH_NONET };

static const uint8_t kMaxLigas = 10;
static const uint8_t kMaxJogos = 8;
static const uint8_t kMenuLiga = 2;  // submenu da liga: Jogos, Tabela
static const uint8_t kVisTab = 4;    // linhas visiveis na tabela unica

static View view = V_LIGAS;
static Fetch fetch_ = FETCH_IDLE;
static uint8_t liga_ = 0;      // qual liga esta aberta
static uint8_t cur = 0;        // cursor (cardapio e lista)
static uint8_t pag_ = 0;  // detail: 0 placar, 1 equipes, 2 gols/estadio
static const uint8_t kPags = 3;
static uint32_t pending_ms_ = 0;
static uint8_t menu_cur = 0;   // submenu da liga: 0 Jogos, 1 Tabela
static uint8_t gcur_ = 0;      // grupo aberto (modo grupos)
static uint8_t tab_off_ = 0;   // 1a linha visivel (modo tabela unica)

static FutLiga ligas_[kMaxLigas];
static uint8_t n_ligas_ = 0;
static CopaJogo jogos_[kMaxJogos];
static uint8_t n_jogos_ = 0;
static FutClass classif_;      // classificacao da liga aberta (/futebol/tabela)
static char buf_[3072];        // payload do server
static char path_[48];         // /futebol/jogos?liga=X da liga aberta

// acompanhamento ao vivo: o detail aberto refaz o GET e dispara o efeito de
// gol; a lista refaz o GET so pra manter o placar das linhas atualizado
static const uint32_t kLiveRefetchMs = 20000;
static const uint32_t kAbreRefetchMs = 1500;  // rele quase na hora ao abrir
static int8_t live_s1_, live_s2_;
static char live_t1_[6], live_t2_[6];
static uint32_t live_next_ = 0;    // 0 = detail nao esta acompanhando
static uint32_t list_next_ = 0;    // 0 = lista sem jogo rolando

static bool alguma_live() {
  for (uint8_t i = 0; i < n_jogos_; i++)
    if (jogos_[i].live) return true;
  return false;
}

static void live_watch(const CopaJogo& j) {
  live_s1_ = j.s1; live_s2_ = j.s2;
  strncpy(live_t1_, j.t1, sizeof(live_t1_));
  strncpy(live_t2_, j.t2, sizeof(live_t2_));
  live_next_ = millis() + kAbreRefetchMs;  // rele quase na hora ao abrir
}

// escolher a liga abre o submenu dela (Jogos / Tabela) — sem rede ainda
static void abre_submenu(uint8_t i) {
  liga_ = i;
  menu_cur = 0;
  list_next_ = 0;
  view = V_LIGA;
}

static void abre_lista() {
  snprintf(path_, sizeof(path_), "/futebol/jogos?liga=%s", ligas_[liga_].id);
  cur = 0;
  fetch_ = wifi::connected() ? FETCH_PENDING : FETCH_NONET;
  pending_ms_ = millis();
  view = V_LIST;
}

static void abre_tabela() {
  snprintf(path_, sizeof(path_), "/futebol/tabela?liga=%s", ligas_[liga_].id);
  gcur_ = 0;
  tab_off_ = 0;
  fetch_ = wifi::connected() ? FETCH_PENDING : FETCH_NONET;
  pending_ms_ = millis();
  view = V_TABELA;
}

static void volta_ligas() {
  view = V_LIGAS;
  cur = liga_;
  fetch_ = FETCH_OK;  // o cardapio ainda esta na RAM: sem refetch
  list_next_ = 0;
}

static void volta_submenu() {  // jogos/tabela voltam pro submenu da liga
  view = V_LIGA;
  list_next_ = 0;
}

static void on_enter() {
  view = V_LIGAS;
  cur = 0;
  // sem WiFi nem tenta o GET: mostra a tela padrao de sem conexao
  fetch_ = wifi::connected() ? FETCH_PENDING : FETCH_NONET;
  pending_ms_ = millis();
  live_next_ = 0;
  list_next_ = 0;
}

// refetch silencioso do jogo aberto: reencontra pelo par de times (a lista
// do server pode mudar de ordem) e compara o placar pra acusar o gol
static void live_refetch(uint32_t now_ms) {
  live_next_ = now_ms + kLiveRefetchMs;
  if (http::get_json(path_, buf_, sizeof(buf_)) != 200) return;
  uint8_t n = copa_parse(buf_, jogos_, kMaxJogos, nullptr);
  if (n == 0) return;  // mantem o que tinha; tenta de novo no proximo ciclo
  n_jogos_ = n;
  for (uint8_t i = 0; i < n_jogos_; i++) {
    const CopaJogo& j = jogos_[i];
    if (strcmp(j.t1, live_t1_) != 0 || strcmp(j.t2, live_t2_) != 0) continue;
    cur = i;
    if (j.s1 > live_s1_ || j.s2 > live_s2_) {
      char placar[24];
      copa_linha(j, placar, sizeof(placar));
      const char* autor = (j.s1 > live_s1_) ? copa_ultimo_gol(j.g1)
                                            : copa_ultimo_gol(j.g2);
      goal_fx::start(placar, autor);
    }
    live_s1_ = j.s1; live_s2_ = j.s2;
    return;
  }
  view = V_LIST;  // o jogo saiu do feed (terminou): volta pra lista
  cur = 0;
  live_next_ = 0;
}

// refetch silencioso da lista: placar ao vivo nas linhas; quando o ultimo
// jogo rolando sai do feed, para de refazer sozinho
static void list_refetch(uint32_t now_ms) {
  list_next_ = now_ms + kLiveRefetchMs;
  if (http::get_json(path_, buf_, sizeof(buf_)) != 200) return;
  uint8_t n = copa_parse(buf_, jogos_, kMaxJogos, nullptr);
  if (n == 0) return;
  n_jogos_ = n;
  if (cur >= n_jogos_) cur = n_jogos_ - 1;
  if (!alguma_live()) list_next_ = 0;
}

static void tick(uint32_t now_ms) {
  // o GET bloqueante seguraria o PWM do bip de tecla ligado por segundos:
  // espera o som acabar (o buzzer::tick do main desliga) antes de buscar
  if (buzzer::busy()) return;
  if (view == V_DETAIL && live_next_ && (int32_t)(now_ms - live_next_) >= 0)
    live_refetch(now_ms);
  if (view == V_LIST && list_next_ && (int32_t)(now_ms - list_next_) >= 0)
    list_refetch(now_ms);
  // espera 1 frame (~50 ms) renderizar "Buscando..." antes de bloquear no GET
  if (fetch_ != FETCH_PENDING || now_ms - pending_ms_ < 60) return;
  if (view == V_LIGAS) {
    int code = http::get_json("/futebol/ligas", buf_, sizeof(buf_));
    n_ligas_ = (code == 200) ? fut_parse_ligas(buf_, ligas_, kMaxLigas) : 0;
    fetch_ = (code == 200) ? FETCH_OK : FETCH_ERR;
    return;
  }
  if (view == V_TABELA) {
    int code = http::get_json(path_, buf_, sizeof(buf_));
    if (code == 200) fut_parse_tabela(buf_, &classif_);
    else classif_.ng = 0;  // erro de rede: cai pra "sem tabela"
    fetch_ = (code == 200) ? FETCH_OK : FETCH_ERR;
    return;
  }
  int code = http::get_json(path_, buf_, sizeof(buf_));
  n_jogos_ = (code == 200) ? copa_parse(buf_, jogos_, kMaxJogos, nullptr) : 0;
  fetch_ = (code == 200) ? FETCH_OK : FETCH_ERR;  // erro fica na tela, sem bip
  list_next_ = (fetch_ == FETCH_OK && alguma_live()) ? now_ms + kLiveRefetchMs
                                                     : 0;
}

static bool input(Button b, BtnEvent e) {
  if (e != EV_PRESS) return false;
  switch (view) {
    case V_LIGAS:
      if (fetch_ == FETCH_PENDING) return true;  // ocupado buscando
      // erro/vazio na raiz: nada a navegar — C nao consumido fecha o app
      if (fetch_ != FETCH_OK || n_ligas_ == 0) return false;
      if (b == BTN_UP) { cur = (cur + n_ligas_ - 1) % n_ligas_; return true; }
      if (b == BTN_DOWN) { cur = (cur + 1) % n_ligas_; return true; }
      if (b == BTN_OK) { abre_submenu(cur); return true; }
      return false;  // C nao consumido → fecha o app
    case V_LIGA:
      if (b == BTN_UP || b == BTN_DOWN) {
        menu_cur = (menu_cur + 1) % kMenuLiga;  // so dois itens: alterna
        return true;
      }
      if (b == BTN_OK) {
        if (menu_cur == 0) abre_lista();
        else abre_tabela();
        return true;
      }
      volta_ligas();  // C volta pro cardapio de ligas
      return true;
    case V_LIST:
      if (fetch_ == FETCH_PENDING) return true;
      if (fetch_ != FETCH_OK || n_jogos_ == 0) { volta_submenu(); return true; }
      if (b == BTN_UP) { cur = (cur + n_jogos_ - 1) % n_jogos_; return true; }
      if (b == BTN_DOWN) { cur = (cur + 1) % n_jogos_; return true; }
      if (b == BTN_OK) {
        view = V_DETAIL;
        pag_ = 0;
        if (jogos_[cur].live) live_watch(jogos_[cur]);
        return true;
      }
      volta_submenu();  // C volta pro submenu da liga
      return true;
    case V_TABELA:
      if (fetch_ == FETCH_PENDING) return true;
      if (fetch_ != FETCH_OK || classif_.ng == 0) { volta_submenu(); return true; }
      if (classif_.ng > 1) {  // grupos: UP/DOWN troca de grupo
        if (b == BTN_UP) { gcur_ = (gcur_ + classif_.ng - 1) % classif_.ng;
                           return true; }
        if (b == BTN_DOWN) { gcur_ = (gcur_ + 1) % classif_.ng; return true; }
      } else {  // tabela unica: UP/DOWN rola a janela
        uint8_t maxoff = classif_.nt > kVisTab ? classif_.nt - kVisTab : 0;
        if (b == BTN_UP) { if (tab_off_) tab_off_--; return true; }
        if (b == BTN_DOWN) { if (tab_off_ < maxoff) tab_off_++; return true; }
      }
      volta_submenu();  // OK/C volta pro submenu
      return true;
    case V_DETAIL: {
      const CopaJogo& j = jogos_[cur];
      if (b == BTN_UP) { pag_ = (pag_ + kPags - 1) % kPags; return true; }
      if (b == BTN_DOWN) { pag_ = (pag_ + 1) % kPags; return true; }
      if (b == BTN_OK) {
        // OK alterna o par de avisos do jogo (inicio + gols, ver jogo_aviso);
        // o vigia consulta o live da liga deste jogo
        char live_path[48];
        snprintf(live_path, sizeof(live_path), "/futebol/live?liga=%s",
                 ligas_[liga_].id);
        jogo_aviso_toggle(j, live_path);
        sound::play(sound::SND_CONFIRM);
        return true;
      }
      view = V_LIST;  // C volta pra lista
      live_next_ = 0;
      return true;
    }
  }
  return false;
}

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_3310_small);
  switch (view) {
    case V_LIGAS: {
      nokia_ui::text_bold_center(g, 8, tr(STR_APP_FUTEBOL));
      if (fetch_ == FETCH_PENDING) {
        g.drawUTF8(2, 24, tr(STR_SEARCHING));
        break;
      }
      if (fetch_ == FETCH_NONET) {
        nokia_ui::no_network(g);
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      if (fetch_ == FETCH_ERR) {
        nokia_ui::text_bold_center(g, 24, tr(STR_NO_RESPONSE));
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      if (n_ligas_ == 0) {  // mundo em pausa: nenhuma liga com jogo na janela
        g.drawUTF8(2, 24, tr(STR_NO_GAMES));
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      const uint8_t kVis = 3;
      uint8_t top = cur >= kVis ? (uint8_t)(cur - kVis + 1) : 0;
      for (uint8_t i = 0; i < kVis && top + i < n_ligas_; i++) {
        int y = 11 + i * 9;
        const FutLiga& l = ligas_[top + i];
        bool sel = (top + i == cur);
        char nome[24];
        snprintf(nome, sizeof(nome), "%s", l.n);
        int fim = 81;  // ate onde o nome pode ir sem invadir o marcador
        char d[8] = "";
        int dx = 0;
        if (l.live) {  // rolando agora: mesma bolinha da lista de jogos
          fim = 76;
        } else if (l.dia) {  // rodada ja passou: reloginho + ultimo jogo
          snprintf(d, sizeof(d), "%u/%u", l.dia, l.mes);
          dx = 81 - (int)g.getStrWidth(d);
          fim = dx - MINI_CLOCK_W - 5;
        }
        nokia_ui::poda(g, nome, fim - 3);
        nokia_ui::list_row(g, y, 84, nome, sel);
        if (sel) g.setDrawColor(0);
        if (l.live) {
          g.drawStr(78, y + 8, "*");
        } else if (l.dia) {
          g.drawStr(dx, y + 8, d);
          g.drawXBMP(dx - MINI_CLOCK_W - 2, y + 2, MINI_CLOCK_W, MINI_CLOCK_H,
                     mini_clock_bits);
        }
        if (sel) g.setDrawColor(1);
      }
      nokia_ui::softkey(g, tr(STR_SELECT));
      break;
    }
    case V_LIGA: {
      // submenu da liga: Jogos / Tabela (espaco pra mais visoes no futuro)
      nokia_ui::text_bold_center(g, 8, ligas_[liga_].n);
      const char* itens[kMenuLiga] = {tr(STR_GAMES), tr(STR_TABLE)};
      for (uint8_t i = 0; i < kMenuLiga; i++) {
        int y = 13 + i * 11;
        if (i == menu_cur) {
          g.drawBox(0, y, 84, 10);
          g.setDrawColor(0);
        }
        g.drawUTF8(4, y + 8, itens[i]);
        g.setDrawColor(1);
      }
      nokia_ui::softkey(g, tr(STR_SELECT));
      break;
    }
    case V_LIST: {
      nokia_ui::text_bold_center(g, 8, ligas_[liga_].n);
      if (fetch_ == FETCH_PENDING) {
        g.drawUTF8(2, 24, tr(STR_SEARCHING));
        break;
      }
      if (fetch_ == FETCH_NONET) {
        nokia_ui::no_network(g);
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      if (fetch_ == FETCH_ERR) {
        nokia_ui::text_bold_center(g, 24, tr(STR_NO_RESPONSE));
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      if (n_jogos_ == 0) {
        g.drawUTF8(2, 24, tr(STR_NO_GAMES));
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      const uint8_t kVis = 3;
      uint8_t top = cur >= kVis ? (uint8_t)(cur - kVis + 1) : 0;
      char linha[24];
      for (uint8_t i = 0; i < kVis && top + i < n_jogos_; i++) {
        int y = 11 + i * 9;
        const CopaJogo& j = jogos_[top + i];
        copa_linha(j, linha, sizeof(linha));
        bool sel = (top + i == cur);
        nokia_ui::list_row(g, y, 84, linha, sel);
        if (sel) g.setDrawColor(0);
        if (j.live)  // bolinha de "ao vivo"
          g.drawStr(78, y + 8, "*");
        else if (j.s1 >= 0)  // reloginho: este ja e historico
          g.drawXBMP(78, y + 2, MINI_CLOCK_W, MINI_CLOCK_H, mini_clock_bits);
        if (sel) g.setDrawColor(1);
      }
      nokia_ui::softkey(g, tr(STR_OPEN));
      break;
    }
    case V_DETAIL: {
      const CopaJogo& j = jogos_[cur];
      jogo_view_detail(g, j, pag_, jogo_aviso_armado(j));
      break;
    }
    case V_TABELA: {
      if (fetch_ == FETCH_PENDING) {
        nokia_ui::text_bold_center(g, 8, tr(STR_TABLE));
        g.drawUTF8(2, 28, tr(STR_SEARCHING));
        break;
      }
      if (fetch_ == FETCH_NONET) {
        nokia_ui::text_bold_center(g, 8, tr(STR_TABLE));
        nokia_ui::no_network(g);
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      if (fetch_ != FETCH_OK || classif_.ng == 0) {
        // mata-mata puro (Copa do Brasil) ou erro: nao ha classificacao
        nokia_ui::text_bold_center(g, 8, ligas_[liga_].n);
        nokia_ui::text_bold_center(g, 28, tr(STR_NO_GAMES));
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      char num[8];
      if (classif_.ng > 1) {
        // fase de grupos: um bloco por vez, navegavel com UP/DOWN
        char titulo[20];
        snprintf(titulo, sizeof(titulo), "%s %s", tr(STR_GROUP),
                 classif_.gnome[gcur_]);
        nokia_ui::text_bold_center(g, 8, titulo);
        g.drawUTF8(34, 16, tr(STR_TBL_PTS));
        g.drawUTF8(50, 16, tr(STR_TBL_JOGOS));
        g.drawUTF8(62, 16, tr(STR_TBL_SALDO));
        uint8_t ini = classif_.gini[gcur_];
        for (uint8_t i = 0; i < classif_.gn[gcur_]; i++) {
          int y = 24 + i * 7;
          const CopaGrupoTime& t = classif_.times[ini + i];
          g.drawStr(2, y, t.c);
          snprintf(num, sizeof(num), "%d", t.pts);
          g.drawStr(34, y, num);
          snprintf(num, sizeof(num), "%d", t.j);
          g.drawStr(50, y, num);
          snprintf(num, sizeof(num), "%+d", t.sg);
          g.drawStr(62, y, num);
        }
      } else {
        // pontos corridos: tabela unica numerada, rolavel com UP/DOWN
        nokia_ui::text_bold_center(g, 8, ligas_[liga_].n);
        g.drawUTF8(40, 16, tr(STR_TBL_PTS));
        g.drawUTF8(54, 16, tr(STR_TBL_JOGOS));
        g.drawUTF8(66, 16, tr(STR_TBL_SALDO));
        for (uint8_t i = 0; i < kVisTab && tab_off_ + i < classif_.nt; i++) {
          uint8_t idx = tab_off_ + i;
          int y = 24 + i * 7;
          const CopaGrupoTime& t = classif_.times[idx];
          snprintf(num, sizeof(num), "%d", idx + 1);
          g.drawStr(1, y, num);
          g.drawStr(13, y, t.c);
          snprintf(num, sizeof(num), "%d", t.pts);
          g.drawStr(40, y, num);
          snprintf(num, sizeof(num), "%d", t.j);
          g.drawStr(54, y, num);
          snprintf(num, sizeof(num), "%+d", t.sg);
          g.drawStr(66, y, num);
        }
        // setinhas quando ha linhas fora da janela (acima/abaixo)
        uint8_t maxoff = classif_.nt > kVisTab ? classif_.nt - kVisTab : 0;
        if (tab_off_) g.drawTriangle(78, 16, 82, 16, 80, 12);
        if (tab_off_ < maxoff) g.drawTriangle(78, 43, 82, 43, 80, 47);
      }
      break;
    }
  }
}

// animacao de selecao: a bola quica e assenta na boca do gol
static const unsigned char* const kAnim[] = {icon_futebol_f1_bits,
                                             icon_futebol_f2_bits, nullptr};
const App app_futebol = {STR_APP_FUTEBOL, on_enter, tick, input, nullptr,
                         render, icon_futebol_bits, kAnim};
