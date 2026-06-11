#include "nokia_ui.h"

namespace nokia_ui {
void text_bold(U8G2& g, int x, int y, const char* s) {
  g.drawStr(x, y, s);
  g.drawStr(x + 1, y, s);
}
void text_bold_center(U8G2& g, int y, const char* s) {
  text_bold(g, 42 - ((int)g.getStrWidth(s) + 1) / 2, y, s);
}
void softkey(U8G2& g, const char* label) {
  g.setFont(u8g2_font_nokiafc22_tr);
  text_bold_center(g, 47, label);
}
}  // namespace nokia_ui
