#include "nokia_ui.h"
#include "assets.h"
#include "fonts3310.h"

namespace nokia_ui {
// negrito real (fonte 3310 small bold), preservando a fonte corrente do caller
void text_bold(U8G2& g, int x, int y, const char* s) {
  const uint8_t* prev = g.getU8g2()->font;
  g.setFont(u8g2_font_3310_small_bold);
  g.drawStr(x, y, s);
  if (prev) g.setFont(prev);
}
void text_bold_center(U8G2& g, int y, const char* s) {
  const uint8_t* prev = g.getU8g2()->font;
  g.setFont(u8g2_font_3310_small_bold);
  g.drawStr(42 - (int)g.getStrWidth(s) / 2, y, s);
  if (prev) g.setFont(prev);
}
int bold_width(U8G2& g, const char* s) {
  const uint8_t* prev = g.getU8g2()->font;
  g.setFont(u8g2_font_3310_small_bold);
  int w = (int)g.getStrWidth(s);
  if (prev) g.setFont(prev);
  return w;
}
void softkey(U8G2& g, const char* label) { text_bold_center(g, 47, label); }
void no_network(U8G2& g) {
  g.drawXBMP((84 - ICON_NOWIFI_W) / 2, 11, ICON_NOWIFI_W, ICON_NOWIFI_H,
             icon_nowifi_bits);
  g.drawStr(42 - (int)g.getStrWidth("Conecte o WiFi") / 2, 38, "Conecte o WiFi");
}
}  // namespace nokia_ui
