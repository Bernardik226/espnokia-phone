#include "statusbar.h"
#include "icons.h"

void statusbar_draw(U8G2& g, const char* clock_str, bool wifi_on) {
  g.setFont(u8g2_font_4x6_tr);
  g.drawXBM(1, 1, 6, 6, wifi_on ? kIconWifiOn : kIconWifiOff);
  g.drawStr(42 - (int)g.getStrWidth(clock_str) / 2, 7, clock_str);
  g.drawXBM(77, 1, 6, 6, kIconBatt);
  g.drawHLine(0, 8, 84);
}
