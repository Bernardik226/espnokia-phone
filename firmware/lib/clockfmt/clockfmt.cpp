#include "clockfmt.h"

void hhmm_format(uint8_t h, uint8_t m, char out[6]) {
  out[0] = (char)('0' + h / 10);
  out[1] = (char)('0' + h % 10);
  out[2] = ':';
  out[3] = (char)('0' + m / 10);
  out[4] = (char)('0' + m % 10);
  out[5] = '\0';
}

void hhmm_format12(uint8_t hour, uint8_t minute, bool fmt24, char out[6], bool* pm) {
  if (pm) *pm = hour >= 12;
  uint8_t h = fmt24 ? hour : (uint8_t)(hour % 12 == 0 ? 12 : hour % 12);
  hhmm_format(h, minute, out);
}

void clock_format(uint32_t ms, char out[6], bool* colon_on) {
  uint32_t total_min = ms / 60000UL;
  hhmm_format((uint8_t)((12 + total_min / 60) % 24), (uint8_t)(total_min % 60), out);
  *colon_on = ((ms / 1000) % 2) == 0;
}

uint8_t days_in_month(uint16_t y, uint8_t m) {
  static const uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
  return (m == 2 && leap) ? 29 : kDays[m - 1];
}

uint8_t date_weekday(uint16_t y, uint8_t m, uint8_t d) {
  static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y--;
  return (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
}
