#include "app_clock.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "clockfmt.h"
#include "i18n.h"
#include "drivers/rtc.h"
#include "ui/assets.h"
#include "ui/nokia_ui.h"

// Estilo Clock (Menu 11) do 3310: hora grande no centro.
// Futuro: alarme/cronometro. A hora pequena fica no standby, como no original.
static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char hhmm[6];
  bool colon = ((millis() / 1000) % 2) == 0;
  rtc::DateTime dt;
  if (rtc::now(dt)) hhmm_format(dt.hour, dt.min, hhmm);
  else clock_format(millis(), hhmm, &colon);

  g.setFont(u8g2_font_VCR_OSD_tn);
  // posicao fixa: desenha "HH:MM" e, no piscar, apaga so o ':' com um box
  int w = (int)g.getStrWidth(hhmm);
  int x = 42 - w / 2;
  g.drawStr(x, 30, hhmm);
  if (!colon) {
    char hh[3] = {hhmm[0], hhmm[1], '\0'};
    g.setDrawColor(0);
    g.drawBox(x + (int)g.getStrWidth(hh), 12, (int)g.getStrWidth(":"), 20);
    g.setDrawColor(1);
  }
  nokia_ui::softkey(g, tr(STR_BACK));
}
const App app_clock = {STR_APP_CLOCK, nullptr, nullptr, nullptr, nullptr, render,
                       icon_clock_bits};
