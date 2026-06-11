#include "btnlogic.h"

BtnEvent btn_step(BtnState& s, bool raw, uint32_t now, uint32_t debounce_ms) {
  if (raw != s.last_raw) { s.last_raw = raw; s.t_change = now; }
  if (raw != s.stable && (now - s.t_change) >= debounce_ms) {
    s.stable = raw;
    return raw ? EV_PRESS : EV_RELEASE;
  }
  return EV_NONE;
}
