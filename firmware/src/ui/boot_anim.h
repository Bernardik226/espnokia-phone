#pragma once
#include <U8g2lib.h>

// Boot estilo Nokia: logo NOKIA + maos convergindo + startup chime.
// Bloqueante (segura ate o fim do chime, timeout 4s); roda buzzer::tick por dentro.
void boot_anim_play(U8G2& g);
