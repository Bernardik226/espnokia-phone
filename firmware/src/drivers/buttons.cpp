#include "buttons.h"
#include <Arduino.h>
#include "pins.h"

namespace buttons {
static const uint8_t kPins[BTN_COUNT] = {PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_OK, PIN_BTN_C};
static BtnState states[BTN_COUNT];
static bool repeating_ = false;

void init() {
  for (uint8_t i = 0; i < BTN_COUNT; i++) pinMode(kPins[i], INPUT_PULLUP);
}
bool poll(uint32_t now, Button& btn_out, BtnEvent& ev_out) {
  repeating_ = false;
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    bool raw = digitalRead(kPins[i]) == LOW;  // ativo-baixo
    BtnEvent ev = btn_step(states[i], raw, now);
    if (ev != EV_NONE) { btn_out = (Button)i; ev_out = ev; return true; }
    // segurar UP/DOWN navega rapido: sintetiza PRESS no ritmo do btn_repeat
    if ((i == BTN_UP || i == BTN_DOWN) && btn_repeat(states[i], now)) {
      repeating_ = true;
      btn_out = (Button)i; ev_out = EV_PRESS; return true;
    }
  }
  return false;
}
bool repeating() { return repeating_; }
}  // namespace buttons
