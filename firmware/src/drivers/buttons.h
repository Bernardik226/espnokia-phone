#pragma once
#include "btnlogic.h"

namespace buttons {
void init();
// Varre os 4 botões; retorna via parâmetros o primeiro evento do ciclo.
// Segurar UP/DOWN repete o PRESS (navegação rápida em todo o sistema).
bool poll(uint32_t now_ms, Button& btn_out, BtnEvent& ev_out);
// true se o último evento do poll foi um auto-repeat (pro bip ficar mudo).
bool repeating();
}
