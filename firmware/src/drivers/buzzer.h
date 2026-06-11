#pragma once
#include <stdint.h>

namespace buzzer {
void init();
void beep(uint16_t freq_hz, uint16_t dur_ms);  // bip de tecla; mudo se tune tocando
void play(const char* rtttl);                  // toca tune, não-bloqueante
void stop();
bool busy();       // qualquer som (beep ou tune)
bool tune_busy();  // só tune RTTTL em andamento
void tick(uint32_t now_ms);                    // chamar todo loop()
}
