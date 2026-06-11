#pragma once
#include <stdint.h>

namespace buzzer {
void init();
void beep(uint16_t freq_hz, uint16_t dur_ms);  // bip único (tecla)
void play(const char* rtttl);                  // toca tune, não-bloqueante
void stop();
bool busy();
void tick(uint32_t now_ms);                    // chamar todo loop()
}
