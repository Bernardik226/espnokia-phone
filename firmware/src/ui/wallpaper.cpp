#include "ui/wallpaper.h"
#include <Preferences.h>
#include "i18n.h"
#include "ui/assets.h"

namespace wallpaper {

enum { W_ESPNOKIA, W_NEUTRO, W_CLAWD, W_PONTOS, W_COUNT };
static Preferences prefs_;
static bool loaded_ = false;
static uint8_t cur_ = W_ESPNOKIA;

static void load() {
  if (loaded_) return;
  prefs_.begin("espnokia");
  cur_ = prefs_.getUChar("wall", W_ESPNOKIA);
  if (cur_ >= W_COUNT) cur_ = W_ESPNOKIA;
  loaded_ = true;
}

uint8_t count() { return W_COUNT; }
uint8_t current() { load(); return cur_; }
void set(uint8_t i) {
  load();
  cur_ = i < W_COUNT ? i : W_ESPNOKIA;
  prefs_.putUChar("wall", cur_);
}
const char* name(uint8_t i) {
  switch (i) {
    case W_ESPNOKIA: return "espnokia";
    case W_NEUTRO:   return tr(STR_WALL_NEUTRO);
    case W_CLAWD:    return "Claw'd";
    default:         return tr(STR_WALL_PONTOS);
  }
}

void draw_at(U8G2& g, uint8_t i) {
  switch (i) {
    case W_ESPNOKIA:  // marca do projeto: emblema eN + wordmark (o de fabrica)
      g.drawXBMP(24, 9, ESPNOKIA_EMBLEM_W, ESPNOKIA_EMBLEM_H, espnokia_emblem_bits);
      g.drawXBMP(2, 31, ESPNOKIA_LOGO_W, ESPNOKIA_LOGO_H, espnokia_logo_bits);
      break;
    case W_NEUTRO:    // limpo: so relogio + sinal, sem arte
      break;
    case W_CLAWD:     // mascote centralizado
      g.drawXBMP((84 - CLAUDE_PET_W) / 2, 13, CLAUDE_PET_W, CLAUDE_PET_H, claude_pet_bits);
      break;
    case W_PONTOS:    // padrao sutil de pontos em losango
      for (int y = 12; y < 40; y += 4)
        for (int x = ((y / 4) & 1) ? 2 : 0; x < 84; x += 4)
          g.drawPixel(x, y);
      break;
  }
}
void draw(U8G2& g) { load(); draw_at(g, cur_); }

}  // namespace wallpaper
