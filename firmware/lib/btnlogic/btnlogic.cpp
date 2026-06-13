#include "btnlogic.h"

BtnEvent btn_step(BtnState& s, bool raw, uint32_t now, uint32_t debounce_ms) {
  if (raw != s.last_raw) { s.last_raw = raw; s.t_change = now; }
  if (raw != s.stable && (now - s.t_change) >= debounce_ms) {
    s.stable = raw;
    return raw ? EV_PRESS : EV_RELEASE;
  }
  return EV_NONE;
}

bool btn_repeat(BtnState& s, uint32_t now, uint32_t delay_ms, uint32_t rate_ms) {
  if (!s.stable) { s.t_repeat = 0; return false; }   // soltou: desagenda
  if (!s.t_repeat) { s.t_repeat = now + delay_ms; return false; }
  if ((int32_t)(now - s.t_repeat) < 0) return false;
  s.t_repeat = now + rate_ms;
  return true;
}
