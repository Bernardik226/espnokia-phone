#include "app_copa.h"
#include <Arduino.h>
#include <string.h>
#include <U8g2lib.h>
#include "alarm.h"
#include "copamodel.h"
#include "i18n.h"
#include "net/http.h"
#include "sound.h"
#include "net/wifi.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Copa 2026 via server companion: V_MENU escolhe a aba, V_LIST mostra a
// janela de 3 jogos, V_DETAIL abre um jogo. O fetch HTTP e bloqueante
// (~ate 5 s), entao fica 1 frame em FETCH_PENDING pra tela "Buscando..."
// aparecer antes — o 3310 tambem "pensava" com a ampulheta na tela.
enum View : uint8_t { V_MENU, V_LIST, V_DETAIL };
enum Fetch : uint8_t { FETCH_IDLE, FETCH_PENDING, FETCH_OK, FETCH_ERR,
                       FETCH_NONET };

static const StrId kAbas[] = {STR_NEXT_GAMES, STR_BRAZIL, STR_LIVE_TAB};
static const char* kPaths[] = {"/copa/proximos?n=8", "/copa/brasil", "/copa/live"};
static const uint8_t kAbaCount = 3;
static const uint8_t kMaxJogos = 8;

static View view = V_MENU;
static Fetch fetch_ = FETCH_IDLE;
static uint8_t aba_ = 0;       // qual lista esta aberta
static uint8_t cur = 0;        // cursor (menu e lista)
static uint32_t pending_ms_ = 0;

static CopaJogo jogos_[kMaxJogos];
static uint8_t n_jogos_ = 0;
static char buf_[2048];        // payload do server (8 jogos ~1.2 KB)

// acompanhamento do jogo aberto em V_DETAIL quando esta ao vivo: refetch
// periodico e, se o placar mudou, alerta de gol (som + "GOL!" piscando)
static const uint32_t kLiveRefetchMs = 45000;
static int8_t live_s1_, live_s2_;
static char live_t1_[6], live_t2_[6];
static uint32_t live_next_ = 0;    // 0 = nao esta acompanhando
static uint32_t goal_flash_ = 0;   // pisca "GOL!" ate este millis

static void live_watch(const CopaJogo& j) {
  live_s1_ = j.s1; live_s2_ = j.s2;
  strncpy(live_t1_, j.t1, sizeof(live_t1_));
  strncpy(live_t2_, j.t2, sizeof(live_t2_));
  live_next_ = millis() + kLiveRefetchMs;
}

static void abre_lista(uint8_t aba) {
  aba_ = aba;
  cur = 0;
  // sem WiFi nem tenta o GET: mostra a tela padrao de sem conexao
  fetch_ = wifi::connected() ? FETCH_PENDING : FETCH_NONET;
  pending_ms_ = millis();
  view = V_LIST;
}

static void on_enter() {
  view = V_MENU;
  fetch_ = FETCH_IDLE;
  cur = 0;
  live_next_ = 0;
}

// refetch silencioso do jogo aberto: reencontra pelo par de times (a lista
// do server pode mudar de ordem) e compara o placar pra acusar o gol
static void live_refetch(uint32_t now_ms) {
  live_next_ = now_ms + kLiveRefetchMs;
  if (http::get_json(kPaths[aba_], buf_, sizeof(buf_)) != 200) return;
  uint8_t n = copa_parse(buf_, jogos_, kMaxJogos, nullptr);
  if (n == 0) return;  // mantem o que tinha; tenta de novo no proximo ciclo
  n_jogos_ = n;
  for (uint8_t i = 0; i < n_jogos_; i++) {
    const CopaJogo& j = jogos_[i];
    if (strcmp(j.t1, live_t1_) != 0 || strcmp(j.t2, live_t2_) != 0) continue;
    cur = i;
    if (j.s1 > live_s1_ || j.s2 > live_s2_) {
      sound::play(sound::SND_ALERT);
      goal_flash_ = now_ms + 4000;
    }
    live_s1_ = j.s1; live_s2_ = j.s2;
    return;
  }
  view = V_LIST;  // o jogo saiu do feed (terminou): volta pra lista
  cur = 0;
  live_next_ = 0;
}

static void tick(uint32_t now_ms) {
  if (view == V_DETAIL && live_next_ && (int32_t)(now_ms - live_next_) >= 0)
    live_refetch(now_ms);
  // espera 1 frame (~50 ms) renderizar "Buscando..." antes de bloquear no GET
  if (fetch_ != FETCH_PENDING || now_ms - pending_ms_ < 60) return;
  int code = http::get_json(kPaths[aba_], buf_, sizeof(buf_));
  n_jogos_ = (code == 200) ? copa_parse(buf_, jogos_, kMaxJogos, nullptr) : 0;
  fetch_ = (code == 200) ? FETCH_OK : FETCH_ERR;  // erro fica so na tela, sem bip
}

static bool input(Button b, BtnEvent e) {
  if (e != EV_PRESS) return false;
  switch (view) {
    case V_MENU:
      if (b == BTN_UP) { cur = (cur + kAbaCount - 1) % kAbaCount; return true; }
      if (b == BTN_DOWN) { cur = (cur + 1) % kAbaCount; return true; }
      if (b == BTN_OK) { abre_lista(cur); return true; }
      return false;  // C nao consumido → shell volta pro menu
    case V_LIST:
      if (fetch_ == FETCH_PENDING) return true;  // ocupado buscando
      if (fetch_ != FETCH_OK || n_jogos_ == 0) { view = V_MENU; cur = aba_; return true; }
      if (b == BTN_UP) { cur = (cur + n_jogos_ - 1) % n_jogos_; return true; }
      if (b == BTN_DOWN) { cur = (cur + 1) % n_jogos_; return true; }
      if (b == BTN_OK) {
        view = V_DETAIL;
        if (jogos_[cur].live) live_watch(jogos_[cur]);
        return true;
      }
      view = V_MENU; cur = aba_;  // C volta
      return true;
    case V_DETAIL: {
      const CopaJogo& j = jogos_[cur];
      if (b == BTN_OK) {  // OK alterna o aviso de inicio do jogo
        if (alarme::armed(j.dia, j.mes, j.h, j.m)) {
          alarme::clear();
          sound::play(sound::SND_CONFIRM);
        } else {
          char lbl[16];
          snprintf(lbl, sizeof(lbl), "%s x %s", j.t1, j.t2);
          alarme::set(STR_GAME_NOW, j.dia, j.mes, j.h, j.m, lbl);
          sound::play(sound::SND_CONFIRM);
        }
        return true;
      }
      view = V_LIST;  // C/UP/DOWN volta pra lista
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
    case V_MENU:
      nokia_ui::text_bold_center(g, 8, tr(STR_APP_COPA));
      for (uint8_t i = 0; i < kAbaCount; i++) {
        int y = 11 + i * 9;
        if (i == cur) {
          g.drawBox(0, y, 84, 9);
          g.setDrawColor(0);
          g.drawUTF8(3, y + 8, tr(kAbas[i]));
          g.setDrawColor(1);
        } else {
          g.drawUTF8(3, y + 8, tr(kAbas[i]));
        }
      }
      nokia_ui::softkey(g, tr(STR_SELECT));
      break;
    case V_LIST: {
      nokia_ui::text_bold_center(g, 8, tr(kAbas[aba_]));
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
        if (j.live) g.drawStr(78, y + 8, "*");  // bolinha de "ao vivo"
        g.setDrawColor(1);
      }
      nokia_ui::softkey(g, tr(STR_OPEN));
      break;
    }
    case V_DETAIL: {
      const CopaJogo& j = jogos_[cur];
      char l1[16], l2[20];
      snprintf(l1, sizeof(l1), "%s x %s", j.t1, j.t2);
      nokia_ui::text_bold_center(g, 8, l1);
      if (j.s1 >= 0 && j.s2 >= 0) {
        snprintf(l2, sizeof(l2), "%d x %d", j.s1, j.s2);
        nokia_ui::text_bold_center(g, 20, l2);
      } else {
        snprintf(l2, sizeof(l2), "%02u/%02u %02u:%02u", j.dia, j.mes, j.h, j.m);
        g.drawStr(42 - (int)g.getStrWidth(l2) / 2, 20, l2);
      }
      g.drawStr(42 - (int)g.getStrWidth(j.info) / 2, 30, j.info);
      bool tem_aviso = alarme::armed(j.dia, j.mes, j.h, j.m);
      bool gol = goal_flash_ && (int32_t)(millis() - goal_flash_) < 0;
      if (gol) {  // acabou de sair gol: "GOL!" piscando no lugar do AO VIVO
        if ((millis() / 250) % 2 == 0) nokia_ui::text_bold_center(g, 39, tr(STR_GOAL));
      } else if (j.live) nokia_ui::text_bold_center(g, 39, tr(STR_LIVE_BIG));
      else if (tem_aviso) g.drawUTF8(42 - (int)g.getUTF8Width(tr(STR_NOTIFY_ON)) / 2, 39, tr(STR_NOTIFY_ON));
      nokia_ui::softkey(g, tr(tem_aviso ? STR_NOTIFY_OFF : STR_NOTIFY));
      break;
    }
  }
}

const App app_copa = {STR_APP_COPA, on_enter, tick, input, nullptr, render,
                      icon_copa_bits};
