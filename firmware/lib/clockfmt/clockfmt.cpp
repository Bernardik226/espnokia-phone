#include "clockfmt.h"

void clock_format(uint32_t ms, char out[6], bool* colon_on) {
  uint32_t total_min = ms / 60000UL;
  uint32_t h = (12 + total_min / 60) % 24;
  uint32_t m = total_min % 60;
  out[0] = (char)('0' + h / 10);
  out[1] = (char)('0' + h % 10);
  out[2] = ':';
  out[3] = (char)('0' + m / 10);
  out[4] = (char)('0' + m % 10);
  out[5] = '\0';
  *colon_on = ((ms / 1000) % 2) == 0;
}
