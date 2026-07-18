#include "game_breakout.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include "breakout.h"
#include "i18n.h"
#include "drivers/buzzer.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Menu -> (Iniciar / Nível / Recorde) -> jogo. O Nível é a dificuldade (1..5,
// igual ao Snake): muda o padrão da fase, a densidade de tijolos duros e a
// velocidade da bola; cada Iniciar sorteia uma fase daquela dificuldade.
// Jogar: UP/DOWN movem a raquete (contínuo), OK lança, C volta ao menu; no
// menu, C sai do jogo. Às vezes um tijolo solta um powerup de raquete larga.
enum VB : uint8_t { MENU, LEVELSEL, PLAY, OVER, WIN };
static VB view;
static Breakout jogo;
static uint16_t recorde;
static uint8_t dif;
static bool novo_recorde;
static bool mov_l, mov_r;
static uint8_t menu_sel;                 // 0=Iniciar, 1=Nível, 2=Recorde
static uint32_t last_ball, last_pad;

static void load_nvs() {
  Preferences p; p.begin("brk", true);
  recorde = p.getUShort("hi", 0);
  dif = p.getUChar("dif", 1);
  p.end();
  if (dif < 1 || dif > 5) dif = 1;
}
static void save_nvs() {
  Preferences p; p.begin("brk", false);
  p.putUShort("hi", recorde); p.putUChar("dif", dif);
  p.end();
}

static void comeca() {
  bo_init(jogo, 84, 48, dif, millis() ? millis() : 1);   // fase sorteada da dificuldade
  novo_recorde = false; mov_l = mov_r = false;
  last_ball = last_pad = millis();
  view = PLAY;
}
static void on_enter() { load_nvs(); menu_sel = 0; view = MENU; }
static void on_exit() { buzzer::stop(); }

static void guarda_recorde() {
  if (jogo.score > recorde) { recorde = jogo.score; novo_recorde = true; }
  save_nvs();
}

static bool on_input(Button b, BtnEvent e) {
  switch (view) {
    case MENU:
      if (e != EV_PRESS) return true;
      if (b == BTN_UP)   { menu_sel = (menu_sel + 2) % 3; return true; }
      if (b == BTN_DOWN) { menu_sel = (menu_sel + 1) % 3; return true; }
      if (b == BTN_OK) {
        if (menu_sel == 0) { buzzer::beep(1200, 40); comeca(); }
        else if (menu_sel == 1) view = LEVELSEL;
        return true;                         // menu_sel==2 (recorde): só mostra
      }
      return false;                          // C: sai do jogo (pra lista)
    case LEVELSEL:
      if (e != EV_PRESS) return true;
      if (b == BTN_UP)   { if (dif < 5) dif++; return true; }
      if (b == BTN_DOWN) { if (dif > 1) dif--; return true; }
      save_nvs(); view = MENU;               // OK ou C confirma e volta
      return true;
    case PLAY:
      if (b == BTN_C) { if (e == EV_PRESS) view = MENU; return true; }   // C volta ao menu
      if (b == BTN_UP)   { mov_l = (e == EV_PRESS); return true; }
      if (b == BTN_DOWN) { mov_r = (e == EV_PRESS); return true; }
      if (b == BTN_OK && e == EV_PRESS) { bo_launch(jogo); return true; }
      return true;
    case OVER:
    case WIN:
      if (e != EV_PRESS) return true;
      if (b == BTN_OK) { comeca(); return true; }
      view = MENU; return true;              // C/outros volta ao menu
  }
  return true;
}

static void on_tick(uint32_t now) {
  if (view != PLAY) return;
  if (now - last_pad >= 22) {                 // raquete mais lenta (era 3px/18ms)
    last_pad = now;
    if (mov_l) bo_move_paddle(jogo, -2);
    if (mov_r) bo_move_paddle(jogo, 2);
  }
  if (now - last_ball < bo_ball_interval(jogo)) return;
  last_ball = now;
  switch (bo_step(jogo)) {
    case BO_BRICK:   buzzer::beep(2200, 10); break;
    case BO_PADDLE:  buzzer::beep(1400, 12); break;
    case BO_POWERUP: buzzer::play("pu:d=32,o=6,b=320:c,e,g"); break;   // bônus: arpejo pra cima
    case BO_LIFE:    buzzer::beep(350, 70); break;
    case BO_OVER:    guarda_recorde();
                     buzzer::play("Over:d=16,o=5,b=200:c6,8p,g,8p,c"); view = OVER; break;
    case BO_WIN:     guarda_recorde();
                     buzzer::play("Win:d=16,o=6,b=200:c,e,g,c7"); view = WIN; break;
    default: break;
  }
}

// --- render ---
static void draw_campo(U8G2& g) {
  // tijolos: normal = cheio, duro = contorno + miolo (fica visivelmente diferente)
  for (uint8_t r = 0; r < BO_ROWS; r++)
    for (uint8_t c = 0; c < BO_COLS; c++) {
      uint8_t v = jogo.bricks[r * BO_COLS + c];
      if (!v) continue;
      int bx = c * jogo.brick_w, by = BO_HEADER + r * BO_BRICK_H;
      int bw = jogo.brick_w - 1, bh = BO_BRICK_H - 1;
      if (v == 2) { g.drawFrame(bx, by, bw, bh); g.drawPixel(bx + bw / 2, by + bh / 2); }
      else g.drawBox(bx, by, bw, bh);
    }
  // powerup caindo (cápsula oca — não confunde com a bola)
  if (jogo.pu_falling) g.drawFrame(jogo.pu_x - 2, jogo.pu_y, 5, 3);
  // raquete arredondada
  g.drawRBox(jogo.paddle_x, jogo.h - 3, jogo.paddle_w, 3, 1);
  // bolinha redonda e maior (era um quadradinho 2x2)
  g.drawDisc(jogo.bx, jogo.by, 1);
}

static void on_render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char buf[12];

  if (view == MENU) {
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_bold_center(g, 8, tr(STR_GAME_BREAKOUT));
    char it1[18], it2[18];
    snprintf(it1, sizeof(it1), "%s: %u", tr(STR_LEVEL), dif);
    snprintf(it2, sizeof(it2), "%s: %u", tr(STR_RECORD), recorde);
    const char* items[3] = {tr(STR_START), it1, it2};
    for (uint8_t i = 0; i < 3; i++) {
      int y = 13 + i * 10;
      bool sel = (i == menu_sel);
      if (sel) { g.drawBox(0, y, 84, 10); g.setDrawColor(0); }
      g.drawUTF8(4, y + 8, items[i]);
      if (sel) g.setDrawColor(1);
    }
    nokia_ui::softkey(g, tr(STR_SELECT));
    return;
  }
  if (view == LEVELSEL) {
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_bold_center(g, 18, tr(STR_LEVEL));
    g.setFont(u8g2_font_logisoso24_tn);
    snprintf(buf, sizeof(buf), "%u", dif);
    int w = g.getStrWidth(buf);
    g.drawStr((84 - w) / 2, 46, buf);
    return;
  }

  // PLAY / OVER / WIN — header: placar / HS★ / vidas, e o campo
  g.setFont(u8g2_font_4x6_tr);
  snprintf(buf, sizeof(buf), "%u", jogo.score);
  g.drawStr(1, 6, buf);
  if (recorde)
    nokia_ui::hiscore(g, (84 - nokia_ui::hiscore_w(g, recorde)) / 2, 0, recorde);
  for (uint8_t i = 0; i < jogo.vidas && i < 6; i++)   // vidas: quadradinhos sem cortar
    g.drawBox(80 - i * 4, 1, 3, 3);
  draw_campo(g);

  if (view != PLAY) {
    g.setDrawColor(0); g.drawBox(8, 18, 68, 22); g.setDrawColor(1);
    g.drawFrame(8, 18, 68, 22);
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_bold_center(g, 29, tr(view == WIN ? STR_YOU_WIN
                                         : (novo_recorde ? STR_NEW_RECORD : STR_GAME_OVER)));
    snprintf(buf, sizeof(buf), "%u", jogo.score);
    nokia_ui::text_bold_center(g, 38, buf);
  } else if (!jogo.launched) {                        // dica de lançar
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_center(g, 40, tr(STR_OK));
  }
}

const App game_breakout = {STR_GAME_BREAKOUT, on_enter, on_tick, on_input,
                           on_exit, on_render, nullptr, nullptr};
