#include "nokia_ui.h"
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
void softkey(U8G2& g, const char* label) { text_bold_center(g, 47, label); }
}  // namespace nokia_ui
