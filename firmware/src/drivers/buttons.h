#pragma once
#include "btnlogic.h"

namespace buttons {
void init();
// Varre os 4 botões; retorna via parâmetros o primeiro evento do ciclo.
bool poll(uint32_t now_ms, Button& btn_out, BtnEvent& ev_out);
}
