#pragma once
#include <stdint.h>

enum Button : uint8_t { BTN_UP = 0, BTN_DOWN, BTN_OK, BTN_C, BTN_COUNT };
enum BtnEvent : int8_t { EV_NONE = -1, EV_PRESS = 0, EV_RELEASE = 1 };

struct BtnState {
  bool stable = false;     // estado debounced (true = pressionado)
  bool last_raw = false;
  uint32_t t_change = 0;
};

// Alimentar com leitura crua a cada loop; retorna evento debounced.
BtnEvent btn_step(BtnState& s, bool raw_pressed, uint32_t now_ms,
                  uint32_t debounce_ms = 25);
