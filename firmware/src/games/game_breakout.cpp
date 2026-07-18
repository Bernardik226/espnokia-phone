#include "game_breakout.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include "breakout.h"
#include "i18n.h"
#include "drivers/buzzer.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Controle: UP=raquete p/ esquerda, DOWN=p/ direita (segurar move contínuo);
// OK=lança a bola; C=sai. OVER/WIN: OK reinicia, C sai. A bola acelera a cada
// 4 rebatidas na raquete (encurta o tick).
enum VB : uint8_t { PLAY, OVER, WIN };
static VB view;
static Breakout jogo;
static uint16_t recorde;
static bool novo_recorde;
static bool mov_l, mov_r;
static uint32_t last_ball, last_pad;

static void load_nvs() { Preferences p; p.begin("brk", true); recorde = p.getUShort("hi", 0); p.end(); }
static void save_nvs() { Preferences p; p.begin("brk", false); p.putUShort("hi", recorde); p.end(); }

static void novo_jogo() {
  bo_init(jogo, 84, 48);
  novo_recorde = false; mov_l = mov_r = false;
  last_ball = last_pad = millis();
  view = PLAY;
}
static void on_enter() { load_nvs(); novo_jogo(); }
static void on_exit() { buzzer::stop(); }

static void guarda_recorde() {
  if (jogo.score > recorde) { recorde = jogo.score; novo_recorde = true; }
  save_nvs();
}

static bool on_input(Button b, BtnEvent e) {
  if (view != PLAY) {
    if (e != EV_PRESS) return true;
    if (b == BTN_OK) { novo_jogo(); return true; }
    return false;                              // C/outros: sai pro submenu
  }
  if (b == BTN_C) { if (e == EV_PRESS) return false; return true; }   // C sai
  if (b == BTN_UP)   { mov_l = (e == EV_PRESS); return true; }
  if (b == BTN_DOWN) { mov_r = (e == EV_PRESS); return true; }
  if (b == BTN_OK && e == EV_PRESS) { bo_launch(jogo); return true; }
  return true;
}

static void on_tick(uint32_t now) {
  if (view != PLAY) return;
  if (now - last_pad >= 18) {                  // raquete: movimento contínuo
    last_pad = now;
    if (mov_l) bo_move_paddle(jogo, -3);
    if (mov_r) bo_move_paddle(jogo, 3);
  }
  uint32_t sp = jogo.paddle_hits / 4;          // acelera a cada 4 rebatidas
  if (sp > 8) sp = 8;
  uint32_t interval = 45 - sp * 3;             // 45 -> 21 ms
  if (now - last_ball < interval) return;
  last_ball = now;
  switch (bo_step(jogo)) {
    case BO_BRICK:  buzzer::beep(2200, 12); break;
    case BO_PADDLE: buzzer::beep(1500, 12); break;
    case BO_LIFE:   buzzer::beep(400, 60); break;
    case BO_OVER:   guarda_recorde();
                    buzzer::play("Over:d=16,o=5,b=200:c6,8p,g,8p,c");
                    view = OVER; break;
    case BO_WIN:    guarda_recorde();
                    buzzer::play("Win:d=16,o=6,b=200:c,e,g,c7");
                    view = WIN; break;
    default: break;
  }
}

static void on_render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char buf[12];
  g.setFont(u8g2_font_4x6_tr);
  snprintf(buf, sizeof(buf), "%u", jogo.score);
  g.drawStr(1, 6, buf);
  if (recorde) {                               // recorde no meio do header
    snprintf(buf, sizeof(buf), "hi%u", recorde);
    int w = g.getStrWidth(buf);
    g.drawStr((84 - w) / 2, 6, buf);
  }
  for (uint8_t i = 0; i < jogo.vidas && i < 6; i++)   // vidas: quadradinhos
    g.drawBox(83 - i * 4, 1, 2, 2);
  // tijolos (1px de folga entre eles)
  for (uint8_t r = 0; r < BO_ROWS; r++)
    for (uint8_t c = 0; c < BO_COLS; c++)
      if (jogo.bricks[r * BO_COLS + c])
        g.drawBox(c * jogo.brick_w, BO_HEADER + r * BO_BRICK_H,
                  jogo.brick_w - 1, BO_BRICK_H - 1);
  g.drawBox(jogo.paddle_x, jogo.h - 3, jogo.paddle_w, 2);   // raquete
  g.drawBox(jogo.bx, jogo.by, 2, 2);                        // bola
  if (!jogo.launched && view == PLAY) {                     // dica de lançar
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_center(g, 40, tr(STR_OK));
  }
  if (view != PLAY) {
    g.setDrawColor(0); g.drawBox(8, 18, 68, 22); g.setDrawColor(1);
    g.drawFrame(8, 18, 68, 22);
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_bold_center(g, 29, tr(view == WIN ? STR_YOU_WIN
                                         : (novo_recorde ? STR_NEW_RECORD : STR_GAME_OVER)));
    snprintf(buf, sizeof(buf), "%u", jogo.score);
    nokia_ui::text_bold_center(g, 38, buf);
  }
}

const App game_breakout = {STR_GAME_BREAKOUT, on_enter, on_tick, on_input,
                           on_exit, on_render, nullptr, nullptr};
