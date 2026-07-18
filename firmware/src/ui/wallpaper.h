#pragma once
#include <U8g2lib.h>

// Wallpaper da tela inicial (fundo do standby). Personalizacao basica,
// persistida na NVS. draw() pinta o fundo do wallpaper atual; draw_at() um
// especifico (pro preview no app Configuracoes). O relogio/sinal/softkey do
// standby ficam SEMPRE por cima — o wallpaper e so a arte central.
namespace wallpaper {
uint8_t count();
uint8_t current();
void set(uint8_t i);              // aplica e persiste
const char* name(uint8_t i);     // nome pra lista (i18n onde faz sentido)
void draw(U8G2& g);              // fundo do wallpaper atual
void draw_at(U8G2& g, uint8_t i);
}  // namespace wallpaper
