#include "game_snake.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include "snakegame.h"
#include "i18n.h"
#include "drivers/buzzer.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Snake II: capa -> menu (Novo jogo / Nivel / Recorde) -> jogo.
// Layout estilo Snake II: placar grande no topo, linha, e o campo embaixo.
// Borda mata (sem wrap); o nivel monta um labirinto cada vez maior.
static const uint8_t kCols = 27, kRows = 10, kCell = 3, kOX = 1, kOY = 17;
static const uint32_t kHoldExitMs = 600;    // segurar C p/ voltar ao menu
static const uint32_t kCoverMs = 1800;      // capa some sozinha

enum View : uint8_t { COVER, MENU, LEVELSEL, PLAY, OVER };
static View view;
static SnakeGame jogo;
static uint8_t nivel;            // 1..9
static uint16_t recorde;
static bool novo_recorde;
static uint32_t last_step, c_press_ms, cover_t0;
static uint8_t menu_sel;         // 0=Novo jogo, 1=Nivel, 2=Recorde

static void load_nvs() {
  Preferences p; p.begin("snake", true);
  recorde = p.getUShort("hi", 0);
  nivel = p.getUChar("lv", 1);
  p.end();
  if (nivel < 1 || nivel > 9) nivel = 1;
}
static void save_nvs() {
  Preferences p; p.begin("snake", false);
  p.putUShort("hi", recorde); p.putUChar("lv", nivel); p.end();
}

// labirinto do nivel: cresce com o nivel, sempre longe do spawn (linha central)
static void build_maze(SnakeGame& g, uint8_t level) {
  snake_clear_walls(g);
  g.wrap = false;                            // Snake II: bater na borda mata
  if (level >= 3) {                          // duas barras horizontais
    for (uint8_t x = 4; x <= 10; x++)  snake_set_wall(g, kCols, x, 2);
    for (uint8_t x = 16; x <= 22; x++) snake_set_wall(g, kCols, x, 7);
  }
  if (level >= 5) {                          // duas barras verticais
    for (uint8_t y = 0; y <= 3; y++)   snake_set_wall(g, kCols, 6, y);
    for (uint8_t y = 6; y <= 9; y++)   snake_set_wall(g, kCols, 20, y);
  }
  if (level >= 7) {                          // pilares centrais
    snake_set_wall(g, kCols, 13, 1); snake_set_wall(g, kCols, 13, 2);
    snake_set_wall(g, kCols, 13, 7); snake_set_wall(g, kCols, 13, 8);
  }
}

static void comeca_partida(uint32_t now) {
  snake_init(jogo, kCols, kRows, now ? now : 1);
  build_maze(jogo, nivel);
  // a comida foi posta antes das paredes existirem -> recoloca se caiu numa
  if (snake_is_wall(jogo, kCols, jogo.fx, jogo.fy))
    snake_place_food(jogo, kCols, kRows);
  novo_recorde = false;
  last_step = now;
  view = PLAY;
}

static void on_enter() { load_nvs(); view = COVER; cover_t0 = millis(); menu_sel = 0; }
static void on_exit() { buzzer::stop(); }

static bool on_input(Button b, BtnEvent e) {
  switch (view) {
    case COVER:
      if (e != EV_PRESS) return true;
      if (b == BTN_C) return false;          // C -> cede o controle (sai do jogo)
      view = MENU;                           // qualquer outra entra no menu
      return true;
    case MENU:
      if (e != EV_PRESS) return true;
      if (b == BTN_UP)   { menu_sel = (menu_sel + 2) % 3; return true; }
      if (b == BTN_DOWN) { menu_sel = (menu_sel + 1) % 3; return true; }
      if (b == BTN_OK) {
        if (menu_sel == 0) { buzzer::beep(1200, 40); comeca_partida(millis()); }
        else if (menu_sel == 1) { view = LEVELSEL; }
        return true;                         // menu_sel == 2 (recorde): so mostra
      }
      return false;                          // C -> cede o controle (volta pra lista)
    case LEVELSEL:
      if (e != EV_PRESS) return true;
      if (b == BTN_UP)   { if (nivel < 9) nivel++; return true; }
      if (b == BTN_DOWN) { if (nivel > 1) nivel--; return true; }
      save_nvs(); view = MENU;               // OK ou C confirma e volta
      return true;
    case PLAY:
      // C: toque curto = esquerda; segurar ~600ms = volta pro menu
      if (b == BTN_C) {
        if (e == EV_PRESS) { c_press_ms = millis(); return true; }
        if (e == EV_RELEASE) {
          if (millis() - c_press_ms >= kHoldExitMs) { view = MENU; return true; }
          snake_set_dir(jogo, DIR_LEFT);
          return true;
        }
        return true;
      }
      if (e != EV_PRESS) return true;
      if (b == BTN_UP)   snake_set_dir(jogo, DIR_UP);
      else if (b == BTN_DOWN) snake_set_dir(jogo, DIR_DOWN);
      else if (b == BTN_OK)   snake_set_dir(jogo, DIR_RIGHT);
      return true;
    case OVER:
      if (e != EV_PRESS) return true;
      if (b == BTN_OK) { comeca_partida(millis()); return true; }
      view = MENU;                           // qualquer outra (inclui C) volta pro menu
      return true;
  }
  return true;
}

static void on_tick(uint32_t now) {
  if (view == COVER) {
    if (now - cover_t0 >= kCoverMs) view = MENU;
    return;
  }
  if (view != PLAY) return;
  if (now - last_step < snake_level_interval_ms(nivel)) return;
  last_step = now;
  SnakeEvent ev = snake_step(jogo, kCols, kRows);
  if (ev == SNK_EAT) {
    buzzer::beep(2200, 25);                            // blip de comer
  } else if (ev == SNK_DIE) {
    if (jogo.score > recorde) { recorde = jogo.score; novo_recorde = true; }
    save_nvs();
    buzzer::play("Over:d=16,o=5,b=200:c6,8p,g,8p,c");  // sequencia descendente
    view = OVER;
  }
}

static void draw_campo(U8G2& g) {
  g.drawFrame(0, 16, 84, 32);
  // paredes do labirinto
  for (uint8_t y = 0; y < kRows; y++)
    for (uint8_t x = 0; x < kCols; x++)
      if (snake_is_wall(jogo, kCols, x, y))
        g.drawBox(kOX + x * kCell, kOY + y * kCell, kCell, kCell);
  // cobra
  for (uint16_t i = 0; i < jogo.len; i++)
    g.drawBox(kOX + jogo.bx[i] * kCell, kOY + jogo.by[i] * kCell, kCell, kCell);
  // comida (cruz piscando)
  if ((millis() / 300) % 2) {
    int fx = kOX + jogo.fx * kCell, fy = kOY + jogo.fy * kCell;
    g.drawHLine(fx, fy + 1, kCell); g.drawVLine(fx + 1, fy, kCell);
  }
}

static void on_render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char buf[20];

  if (view == COVER) {
    // capa autoral (logo + cobra enrolada) + logo espnokia no rodape
    g.drawXBMP(0, 0, SNAKE2_COVER_W, SNAKE2_COVER_H, snake2_cover_bits);
    g.drawXBMP((84 - ESPNOKIA_LOGO_W) / 2, 40, ESPNOKIA_LOGO_W, ESPNOKIA_LOGO_H,
               espnokia_logo_bits);
    return;
  }

  if (view == MENU) {
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_bold_center(g, 8, tr(STR_GAME_SNAKE));
    char it1[20], it2[20];
    snprintf(it1, sizeof(it1), "%s: %u", tr(STR_LEVEL), nivel);
    snprintf(it2, sizeof(it2), "%s: %u", tr(STR_RECORD), recorde);
    const char* items[3] = {tr(STR_NEW_GAME), it1, it2};
    for (uint8_t i = 0; i < 3; i++) {
      int y = 11 + i * 9;
      bool sel = (i == menu_sel);
      if (sel) { g.drawBox(0, y, 80, 9); g.setDrawColor(0); }
      g.drawUTF8(3, y + 8, items[i]);
      if (sel) g.setDrawColor(1);
    }
    nokia_ui::softkey(g, tr(STR_SELECT));
    return;
  }

  if (view == LEVELSEL) {
    g.setFont(u8g2_font_3310_small);
    snprintf(buf, sizeof(buf), "%s %u", tr(STR_RECORD), recorde);
    g.drawUTF8(2, 8, buf);                             // recorde pequeno no topo
    nokia_ui::text_bold_center(g, 18, tr(STR_LEVEL));
    g.setFont(u8g2_font_logisoso24_tn);               // numero grande do nivel
    snprintf(buf, sizeof(buf), "%u", nivel);
    int w = g.getStrWidth(buf);
    g.drawStr((84 - w) / 2, 46, buf);
    return;
  }

  // PLAY / OVER — placar grande tipo "0036" + linha + campo
  g.setFont(u8g2_font_7x13B_tr);
  snprintf(buf, sizeof(buf), "%04u", jogo.score);
  g.drawStr(2, 12, buf);
  g.drawHLine(0, 14, 84);
  draw_campo(g);
  if (view == OVER) {
    g.setDrawColor(0); g.drawBox(8, 20, 68, 23); g.setDrawColor(1);
    g.drawFrame(8, 20, 68, 23);
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_bold_center(g, 31, tr(novo_recorde ? STR_NEW_RECORD : STR_GAME_OVER));
    snprintf(buf, sizeof(buf), "%u", jogo.score);
    nokia_ui::text_bold_center(g, 41, buf);
  }
}

const App game_snake = {STR_GAME_SNAKE, on_enter, on_tick, on_input, on_exit,
                        on_render, nullptr, nullptr};
