#pragma once
#include <stdint.h>
#include "btnlogic.h"

// Alarme de jogo: um aviso por vez, persistido na NVS (sobrevive reboot).
// tick() compara com o RTC numa janela de 5 min; ao disparar vira overlay
// fullscreen + Nokia tune em loop ate OK/C. Enquanto ativo, input() engole
// todas as teclas (o main consulta antes de repassar pro shell).
namespace alarme {
void init();
void set(uint8_t dia, uint8_t mes, uint8_t h, uint8_t m, const char* label);
void clear();
bool armed(uint8_t dia, uint8_t mes, uint8_t h, uint8_t m);  // aviso deste jogo?
bool active();                      // overlay tocando agora
void tick(uint32_t now_ms);
bool input(Button b, BtnEvent e);   // true = consumiu (overlay ativo)
void render(void* gfx);             // desenhar DEPOIS do render do app
}  // namespace alarme
