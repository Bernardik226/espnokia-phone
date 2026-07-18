#include "game_2048.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include "game2048.h"
#include "i18n.h"
#include "drivers/buzzer.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Controle: UP=cima, DOWN=baixo, OK=direita, C(toque)=esquerda;
// segurar C ~600ms sai pro submenu. OVER: OK reinicia, C sai.
static const uint32_t kHoldExitMs = 600;
enum V2 : uint8_t { PLAY, OVER };
static V2 view;
static Game2048 jogo;
static uint32_t recorde;
static bool novo_recorde;
static uint32_t c_press_ms;

static void load_nvs() { Preferences p; p.begin("g2048", true); recorde = p.getULong("hi", 0); p.end(); }
static void save_nvs() { Preferences p; p.begin("g2048", false); p.putULong("hi", recorde); p.end(); }

static void novo_jogo() {
  g2048_init(jogo, millis() ? millis() : 1);
  novo_recorde = false;
  view = PLAY;
}
static void on_enter() { load_nvs(); novo_jogo(); }
static void on_exit() { buzzer::stop(); }

static void fim_se_travou() {
  if (!g2048_can_move(jogo)) {
    if (jogo.score > recorde) { recorde = jogo.score; novo_recorde = true; }
    save_nvs();
    buzzer::play("Over:d=16,o=5,b=200:c6,8p,g,8p,c");
    view = OVER;
  }
}
static void aplica(G2Dir d) {
  if (g2048_move(jogo, d)) { buzzer::beep(2000, 12); fim_se_travou(); }
}

static bool on_input(Button b, BtnEvent e) {
  if (view == OVER) {
    if (e != EV_PRESS) return true;
    if (b == BTN_OK) { novo_jogo(); return true; }
    return false;                              // C/outros: cede o controle (sai)
  }
  // PLAY — C acumula esquerda / segurar sai
  if (b == BTN_C) {
    if (e == EV_PRESS) { c_press_ms = millis(); return true; }
    if (e == EV_RELEASE) {
      if (millis() - c_press_ms >= kHoldExitMs) return false;   // segurar C sai
      aplica(G2_LEFT);
      return true;
    }
    return true;
  }
  if (e != EV_PRESS) return true;
  if (b == BTN_UP) aplica(G2_UP);
  else if (b == BTN_DOWN) aplica(G2_DOWN);
  else if (b == BTN_OK) aplica(G2_RIGHT);
  return true;
}

static void on_tick(uint32_t) {}

static void draw_grid(U8G2& g) {
  g.setFont(u8g2_font_4x6_tr);
  for (uint8_t y = 0; y < 4; y++)
    for (uint8_t x = 0; x < 4; x++) {
      int px = 2 + x * 20, py = 16 + y * 8;
      g.drawFrame(px, py, 20, 8);
      uint8_t e = jogo.cell[y * 4 + x];
      if (e) {
        char b[7]; snprintf(b, sizeof(b), "%u", 1u << e);
        int w = g.getStrWidth(b);
        g.drawStr(px + (20 - w) / 2, py + 6, b);
      }
    }
}

static void on_render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char buf[16];
  g.setFont(u8g2_font_7x13B_tr);
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)jogo.score);
  g.drawStr(2, 12, buf);
  g.setFont(u8g2_font_4x6_tr);
  snprintf(buf, sizeof(buf), "hi %lu", (unsigned long)recorde);
  int w = g.getStrWidth(buf);
  g.drawStr(84 - w - 1, 6, buf);
  g.drawHLine(0, 14, 84);
  draw_grid(g);
  if (view == OVER) {
    g.setDrawColor(0); g.drawBox(8, 18, 68, 22); g.setDrawColor(1);
    g.drawFrame(8, 18, 68, 22);
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_bold_center(g, 29, tr(novo_recorde ? STR_NEW_RECORD : STR_GAME_OVER));
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)jogo.score);
    nokia_ui::text_bold_center(g, 38, buf);
  }
}

const App game_2048 = {STR_GAME_2048, on_enter, on_tick, on_input, on_exit,
                       on_render, nullptr, nullptr};
