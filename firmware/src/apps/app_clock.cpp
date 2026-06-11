#include "app_clock.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "clockfmt.h"
#include "ui/assets.h"
#include "ui/nokia_ui.h"

// Estilo Clock (Menu 11) do 3310: hora grande no centro.
// Futuro: alarme/cronometro. A hora pequena fica no standby, como no original.
static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char hhmm[6]; bool colon;
  clock_format(millis(), hhmm, &colon);
  if (!colon) hhmm[2] = ' ';
  g.setFont(u8g2_font_VCR_OSD_tn);
  g.drawStr(42 - (int)g.getStrWidth(hhmm) / 2, 30, hhmm);
  nokia_ui::softkey(g, "Voltar");
}
const App app_clock = {"Relogio", nullptr, nullptr, nullptr, nullptr, render,
                       icon_clock_bits};
