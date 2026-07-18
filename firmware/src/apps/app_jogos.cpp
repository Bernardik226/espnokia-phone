#include "app_jogos.h"
#include <U8g2lib.h>
#include "i18n.h"
#include "games/game_snake.h"
#include "games/game_2048.h"
#include "games/game_breakout.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Cada jogo e um App; o app_jogos faz o submenu e delega ao jogo ativo.
static const App* const kGames[] = {&game_snake, &game_2048, &game_breakout};
static const uint8_t kCount = sizeof(kGames) / sizeof(kGames[0]);

enum View : uint8_t { LIST, PLAYING };
static View view = LIST;
static uint8_t cur = 0;
static const App* jogo = nullptr;

static void on_enter() { view = LIST; cur = 0; }
static void on_exit() {
  if (jogo && jogo->on_exit) jogo->on_exit();
  jogo = nullptr; view = LIST;
}
static void on_tick(uint32_t now) {
  if (view == PLAYING && jogo && jogo->on_tick) jogo->on_tick(now);
}

static void abre(uint8_t i) {
  jogo = kGames[i];
  if (jogo->on_enter) jogo->on_enter();
  view = PLAYING;
}
static void volta_lista() {
  if (jogo && jogo->on_exit) jogo->on_exit();
  jogo = nullptr; view = LIST;
}

static bool on_input(Button b, BtnEvent e) {
  if (view == PLAYING) {
    bool consumiu = jogo && jogo->on_input ? jogo->on_input(b, e) : false;
    if (!consumiu && b == BTN_C) volta_lista();   // jogo cedeu o controle
    return true;                                  // PLAYING segura o shell (nao sai no meio)
  }
  // LIST
  if (e != EV_PRESS) return false;
  if (b == BTN_UP)   { cur = (cur + kCount - 1) % kCount; return true; }
  if (b == BTN_DOWN) { cur = (cur + 1) % kCount; return true; }
  if (b == BTN_OK)   { abre(cur); return true; }
  return false;  // C -> shell volta pro launcher
}

static void on_render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  if (view == PLAYING) { if (jogo && jogo->on_render) jogo->on_render(gfx); return; }
  // LIST no estilo do app_tones (header bold, item selecionado invertido, softkey)
  g.setFont(u8g2_font_3310_small);
  nokia_ui::text_bold_center(g, 8, tr(STR_APP_GAMES));
  const uint8_t kVisible = 3;
  for (uint8_t row = 0; row < kVisible && row < kCount; row++) {
    uint8_t i = row;
    int y = 11 + row * 9;
    bool sel = (i == cur);
    // sem scrollbar aqui (so 1 jogo hoje): barra cheia de 84px, nao 80
    // (item 3 do sweep — unica mudanca visual desta tarefa)
    nokia_ui::list_row(g, y, 84, tr((StrId)kGames[i]->name_id), sel);
  }
  nokia_ui::softkey(g, tr(STR_SELECT));
}

// animacao da gaveta: joystick tomba esquerda -> direita -> descansa no centro
static const unsigned char* const kAnim[] = {icon_jogos_f1_bits,
                                             icon_jogos_f2_bits, nullptr};

const App app_jogos = {STR_APP_GAMES, on_enter, on_tick, on_input, on_exit,
                       on_render, icon_jogos_bits, kAnim};
