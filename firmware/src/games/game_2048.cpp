#include "game_2048.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include "game2048.h"
#include "i18n.h"
#include "drivers/buzzer.h"
#include "sound.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Menu -> (Iniciar / Score / Config) -> jogo.
// Jogar: UP=cima, DOWN=baixo, OK=direita, C(toque)=esquerda; segurar C ~600ms
// volta ao menu; no menu, C sai do jogo. Config tem só mudo/liga do som.
static const uint32_t kHoldExitMs = 600;
enum V2 : uint8_t { MENU, PLAY, OVER, SCORE, CONFIG };
static V2 view;
static Game2048 jogo;
static uint32_t recorde;
static bool novo_recorde;
static uint8_t menu_sel;          // 0=Iniciar, 1=Score, 2=Config
static uint32_t c_press_ms;

// o som respeita o mute GERAL do sistema (buzzer/sound) — o Config só alterna ele
static void load_nvs() {
  Preferences p; p.begin("g2048", true);
  recorde = p.getULong("hi", 0);
  p.end();
}
static void save_nvs() {
  Preferences p; p.begin("g2048", false);
  p.putULong("hi", recorde);
  p.end();
}

static void comeca() {
  g2048_init(jogo, millis() ? millis() : 1);
  novo_recorde = false;
  view = PLAY;
}
static void on_enter() { load_nvs(); menu_sel = 0; view = MENU; }
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
  uint32_t antes = jogo.score;
  if (g2048_move(jogo, d)) {
    if (jogo.score > antes) buzzer::beep(1760, 28);   // fundiu: nota mais alta e cheia
    else buzzer::beep(880, 16);                        // só deslizou: clique seco
    fim_se_travou();
  }
}

static bool on_input(Button b, BtnEvent e) {
  switch (view) {
    case MENU:
      if (e != EV_PRESS) return true;
      if (b == BTN_UP)   { menu_sel = (menu_sel + 2) % 3; return true; }
      if (b == BTN_DOWN) { menu_sel = (menu_sel + 1) % 3; return true; }
      if (b == BTN_OK) {
        if (menu_sel == 0) { buzzer::beep(1200, 40); comeca(); }
        else if (menu_sel == 1) view = SCORE;
        else view = CONFIG;
        return true;
      }
      return false;                            // C: sai do jogo (pra lista)
    case PLAY:
      if (b == BTN_C) {
        if (e == EV_PRESS) { c_press_ms = millis(); return true; }
        if (e == EV_RELEASE) {
          if (millis() - c_press_ms >= kHoldExitMs) { view = MENU; return true; }  // segurar C volta ao menu
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
    case OVER:
      if (e != EV_PRESS) return true;
      if (b == BTN_OK) { comeca(); return true; }
      view = MENU; return true;                // C/outros volta ao menu
    case SCORE:
      if (e != EV_PRESS) return true;
      view = MENU; return true;                // qualquer tecla volta
    case CONFIG:
      if (e != EV_PRESS) return true;
      if (b == BTN_C) { view = MENU; return true; }
      sound::set_muted(!sound::muted());       // OK/UP/DOWN alternam o mute do sistema
      if (!sound::muted()) buzzer::beep(1500, 30);
      return true;
  }
  return true;
}

static void on_tick(uint32_t) {}

static void draw_grid(U8G2& g) {
  g.setFont(u8g2_font_5x8_tr);                 // números maiores (era 4x6, ilegível)
  for (uint8_t y = 0; y < 4; y++)
    for (uint8_t x = 0; x < 4; x++) {
      int px = x * 21, py = 10 + y * 9;         // célula 21x10, matriz ocupa a tela
      g.drawFrame(px, py, 21, 10);
      uint8_t e = jogo.cell[y * 4 + x];
      if (e) {
        char b[7]; snprintf(b, sizeof(b), "%u", 1u << e);
        int w = g.getStrWidth(b);
        g.drawStr(px + (21 - w) / 2, py + 8, b);
      }
    }
}

static void on_render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char buf[16];

  if (view == MENU) {
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_bold_center(g, 9, tr(STR_GAME_2048));
    const char* it[3] = {tr(STR_START), tr(STR_RECORD), tr(STR_OPTIONS)};
    for (uint8_t i = 0; i < 3; i++) {
      int y = 13 + i * 10;
      bool sel = (i == menu_sel);
      if (sel) { g.drawBox(0, y, 84, 10); g.setDrawColor(0); }
      g.drawUTF8(4, y + 8, it[i]);
      if (sel) g.setDrawColor(1);
    }
    nokia_ui::softkey(g, tr(STR_SELECT));
    return;
  }
  if (view == SCORE) {
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_bold_center(g, 14, tr(STR_RECORD));
    g.setFont(u8g2_font_logisoso24_tn);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)recorde);
    int w = g.getStrWidth(buf);
    g.drawStr((84 - w) / 2, 46, buf);
    return;
  }
  if (view == CONFIG) {
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_bold_center(g, 14, tr(STR_SOUND));
    const char* estado = sound::muted() ? tr(STR_MUTE) : "ON";
    g.setFont(u8g2_font_7x13B_tr);
    int w = g.getStrWidth(estado);
    g.drawStr((84 - w) / 2, 34, estado);
    g.setFont(u8g2_font_3310_small);
    nokia_ui::softkey(g, tr(STR_CHANGE));
    return;
  }

  // PLAY / OVER — header compacto: score à esquerda, badge HS★ à direita
  g.setFont(u8g2_font_5x8_tr);
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)jogo.score);
  g.drawStr(1, 8, buf);
  nokia_ui::hiscore(g, 84 - nokia_ui::hiscore_w(g, recorde), 0, recorde);
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
