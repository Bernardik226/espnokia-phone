#include "app_clock.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "clockfmt.h"

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char hhmm[6]; bool colon;
  clock_format(millis(), hhmm, &colon);
  if (!colon) hhmm[2] = ' ';
  g.setFont(u8g2_font_profont22_tn);
  g.drawStr(42 - (int)g.getStrWidth(hhmm) / 2, 32, hhmm);
  g.setFont(u8g2_font_4x6_tr);
  g.drawStr(42 - (int)g.getStrWidth("espnokia") / 2, 45, "espnokia");
}
const App app_clock = {"Relogio", nullptr, nullptr, nullptr, nullptr, render};
