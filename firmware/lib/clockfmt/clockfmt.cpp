#include "clockfmt.h"

void hhmm_format(uint8_t h, uint8_t m, char out[6]) {
  out[0] = (char)('0' + h / 10);
  out[1] = (char)('0' + h % 10);
  out[2] = ':';
  out[3] = (char)('0' + m / 10);
  out[4] = (char)('0' + m % 10);
  out[5] = '\0';
}

void clock_format(uint32_t ms, char out[6], bool* colon_on) {
  uint32_t total_min = ms / 60000UL;
  hhmm_format((uint8_t)((12 + total_min / 60) % 24), (uint8_t)(total_min % 60), out);
  *colon_on = ((ms / 1000) % 2) == 0;
}
