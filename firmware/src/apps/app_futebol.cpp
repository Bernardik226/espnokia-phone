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
enum View : uint8_t { V_LIGAS, V_LIST, V_DETAIL };
enum Fetch : uint8_t { FETCH_IDLE, FETCH_PENDING, FETCH_OK, FETCH_ERR,
                       FETCH_NONET };

static const uint8_t kMaxLigas = 10;
static const uint8_t kMaxJogos = 8;

static View view = V_LIGAS;
static Fetch fetch_ = FETCH_IDLE;
static uint8_t liga_ = 0;      // qual liga esta aberta
static uint8_t cur = 0;        // cursor (cardapio e lista)
static uint8_t pag_ = 0;  // detail: 0 placar, 1 equipes, 2 gols/estadio
static const uint8_t kPags = 3;
static uint32_t pending_ms_ = 0;

static FutLiga ligas_[kMaxLigas];
static uint8_t n_ligas_ = 0;
static CopaJogo jogos_[kMaxJogos];
static uint8_t n_jogos_ = 0;
static char buf_[3072];        // payload do server
static char path_[48];         // /futebol/jogos?liga=X da liga aberta

// acompanhamento ao vivo: o detail aberto refaz o GET e dispara o efeito de
// gol; a lista refaz o GET so pra manter o placar das linhas atualizado
static const uint32_t kLiveRefetchMs = 45000;
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
  live_next_ = millis() + kLiveRefetchMs;
}

static void abre_lista(uint8_t i) {
  liga_ = i;
  snprintf(path_, sizeof(path_), "/futebol/jogos?liga=%s", ligas_[i].id);
  cur = 0;
  fetch_ = wifi::connected() ? FETCH_PENDING : FETCH_NONET;
  pending_ms_ = millis();
  view = V_LIST;
}

static void volta_ligas() {
  view = V_LIGAS;
  cur = liga_;
  fetch_ = FETCH_OK;  // o cardapio ainda esta na RAM: sem refetch
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
      if (b == BTN_OK) { abre_lista(cur); return true; }
      return false;  // C nao consumido → fecha o app
    case V_LIST:
      if (fetch_ == FETCH_PENDING) return true;
      if (fetch_ != FETCH_OK || n_jogos_ == 0) { volta_ligas(); return true; }
      if (b == BTN_UP) { cur = (cur + n_jogos_ - 1) % n_jogos_; return true; }
      if (b == BTN_DOWN) { cur = (cur + 1) % n_jogos_; return true; }
      if (b == BTN_OK) {
        view = V_DETAIL;
        pag_ = 0;
        if (jogos_[cur].live) live_watch(jogos_[cur]);
        return true;
      }
      volta_ligas();  // C volta
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
        if (top + i == cur) {
          g.drawBox(0, y, 84, 9);
          g.setDrawColor(0);
        }
        char nome[24];
        snprintf(nome, sizeof(nome), "%s", l.n);
        int fim = 81;  // ate onde o nome pode ir sem invadir o marcador
        if (l.live) {  // rolando agora: mesma bolinha da lista de jogos
          g.drawStr(78, y + 8, "*");
          fim = 76;
        } else if (l.dia) {  // rodada ja passou: reloginho + ultimo jogo
          char d[8];
          snprintf(d, sizeof(d), "%u/%u", l.dia, l.mes);
          int x = 81 - (int)g.getStrWidth(d);
          g.drawStr(x, y + 8, d);
          g.drawXBMP(x - MINI_CLOCK_W - 2, y + 2, MINI_CLOCK_W, MINI_CLOCK_H,
                     mini_clock_bits);
          fim = x - MINI_CLOCK_W - 5;
        }
        nokia_ui::poda(g, nome, fim - 3);
        g.drawUTF8(3, y + 8, nome);
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
        if (top + i == cur) {
          g.drawBox(0, y, 84, 9);
          g.setDrawColor(0);
        }
        g.drawStr(3, y + 8, linha);
        if (j.live)  // bolinha de "ao vivo"
          g.drawStr(78, y + 8, "*");
        else if (j.s1 >= 0)  // reloginho: este ja e historico
          g.drawXBMP(78, y + 2, MINI_CLOCK_W, MINI_CLOCK_H, mini_clock_bits);
        g.setDrawColor(1);
      }
      nokia_ui::softkey(g, tr(STR_OPEN));
      break;
    }
    case V_DETAIL: {
      const CopaJogo& j = jogos_[cur];
      jogo_view_detail(g, j, pag_, jogo_aviso_armado(j));
      break;
    }
  }
}

// animacao de selecao: a bola quica e assenta na boca do gol
static const unsigned char* const kAnim[] = {icon_futebol_f1_bits,
                                             icon_futebol_f2_bits, nullptr};
const App app_futebol = {STR_APP_FUTEBOL, on_enter, tick, input, nullptr,
                         render, icon_futebol_bits, kAnim};
