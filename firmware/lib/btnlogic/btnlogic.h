#pragma once
#include <stdint.h>

enum Button : uint8_t { BTN_UP = 0, BTN_DOWN, BTN_OK, BTN_C, BTN_COUNT };
enum BtnEvent : int8_t { EV_NONE = -1, EV_PRESS = 0, EV_RELEASE = 1 };

struct BtnState {
  bool stable = false;     // estado debounced (true = pressionado)
  bool last_raw = false;
  uint32_t t_change = 0;
  uint32_t t_repeat = 0;   // proximo auto-repeat agendado (0 = nenhum)
};

// Alimentar com leitura crua a cada loop; retorna evento debounced.
BtnEvent btn_step(BtnState& s, bool raw_pressed, uint32_t now_ms,
                  uint32_t debounce_ms = 25);

// Auto-repeat de quem fica segurando: chamar a cada loop depois do btn_step;
// true = sintetizar um PRESS. O primeiro vem apos delay_ms, dai um por rate_ms.
bool btn_repeat(BtnState& s, uint32_t now_ms, uint32_t delay_ms = 450,
                uint32_t rate_ms = 110);
