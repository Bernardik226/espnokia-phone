#include "app_copa.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "alarm.h"
#include "copamodel.h"
#include "drivers/buzzer.h"
#include "net/http.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Copa 2026 via server companion: V_MENU escolhe a aba, V_LIST mostra a
// janela de 3 jogos, V_DETAIL abre um jogo. O fetch HTTP e bloqueante
// (~ate 5 s), entao fica 1 frame em FETCH_PENDING pra tela "Buscando..."
// aparecer antes — o 3310 tambem "pensava" com a ampulheta na tela.
enum View : uint8_t { V_MENU, V_LIST, V_DETAIL };
enum Fetch : uint8_t { FETCH_IDLE, FETCH_PENDING, FETCH_OK, FETCH_ERR };

static const char* kAbas[] = {"Proximos", "Brasil", "Ao vivo"};
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

static void abre_lista(uint8_t aba) {
  aba_ = aba;
  cur = 0;
  fetch_ = FETCH_PENDING;
  pending_ms_ = millis();
  view = V_LIST;
}

static void on_enter() {
  view = V_MENU;
  fetch_ = FETCH_IDLE;
  cur = 0;
}

static void tick(uint32_t now_ms) {
  // espera 1 frame (~50 ms) renderizar "Buscando..." antes de bloquear no GET
  if (fetch_ != FETCH_PENDING || now_ms - pending_ms_ < 60) return;
  int code = http::get_json(kPaths[aba_], buf_, sizeof(buf_));
  n_jogos_ = (code == 200) ? copa_parse(buf_, jogos_, kMaxJogos, nullptr) : 0;
  if (code == 200) {
    fetch_ = FETCH_OK;
  } else {
    fetch_ = FETCH_ERR;
    buzzer::beep(220, 250);  // beep triste de rede
  }
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
      if (b == BTN_OK) { view = V_DETAIL; return true; }
      view = V_MENU; cur = aba_;  // C volta
      return true;
    case V_DETAIL: {
      const CopaJogo& j = jogos_[cur];
      if (b == BTN_OK) {  // OK alterna o aviso de inicio do jogo
        if (alarme::armed(j.dia, j.mes, j.h, j.m)) {
          alarme::clear();
        } else {
          char lbl[16];
          snprintf(lbl, sizeof(lbl), "%s x %s", j.t1, j.t2);
          alarme::set(j.dia, j.mes, j.h, j.m, lbl);
        }
        return true;
      }
      view = V_LIST;  // C/UP/DOWN volta pra lista
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
      nokia_ui::text_bold_center(g, 8, "Copa 2026");
      for (uint8_t i = 0; i < kAbaCount; i++) {
        int y = 11 + i * 9;
        if (i == cur) {
          g.drawBox(0, y, 84, 9);
          g.setDrawColor(0);
          g.drawStr(3, y + 7, kAbas[i]);
          g.setDrawColor(1);
        } else {
          g.drawStr(3, y + 7, kAbas[i]);
        }
      }
      nokia_ui::softkey(g, "Selecionar");
      break;
    case V_LIST: {
      nokia_ui::text_bold_center(g, 8, kAbas[aba_]);
      if (fetch_ == FETCH_PENDING) {
        g.drawStr(2, 24, "Buscando...");
        break;
      }
      if (fetch_ == FETCH_ERR) {
        nokia_ui::text_bold_center(g, 24, "REDE OCUPADA");
        nokia_ui::softkey(g, "Voltar");
        break;
      }
      if (n_jogos_ == 0) {
        g.drawStr(2, 24, "Nenhum jogo");
        nokia_ui::softkey(g, "Voltar");
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
        g.drawStr(3, y + 7, linha);
        if (j.live) g.drawStr(78, y + 7, "*");  // bolinha de "ao vivo"
        g.setDrawColor(1);
      }
      nokia_ui::softkey(g, "Abrir");
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
      if (j.live) nokia_ui::text_bold_center(g, 39, "AO VIVO");
      else if (tem_aviso) g.drawStr(42 - (int)g.getStrWidth("Aviso ON") / 2, 39, "Aviso ON");
      nokia_ui::softkey(g, tem_aviso ? "Tirar aviso" : "Avisar");
      break;
    }
  }
}

const App app_copa = {"Copa 2026", on_enter, tick, input, nullptr, render,
                      icon_copa_bits};
