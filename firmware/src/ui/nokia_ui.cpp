#include "nokia_ui.h"
#include <string.h>
#include "assets.h"
#include "fonts3310.h"
#include "i18n.h"

namespace nokia_ui {
// negrito real (fonte 3310 small bold), preservando a fonte corrente do
// caller; drawUTF8 em vez de drawStr pra aceitar os acentos das traducoes
void text_bold(U8G2& g, int x, int y, const char* s) {
  const uint8_t* prev = g.getU8g2()->font;
  g.setFont(u8g2_font_3310_small_bold);
  g.drawUTF8(x, y, s);
  if (prev) g.setFont(prev);
}
void text_bold_center(U8G2& g, int y, const char* s) {
  const uint8_t* prev = g.getU8g2()->font;
  g.setFont(u8g2_font_3310_small_bold);
  g.drawUTF8(42 - (int)g.getUTF8Width(s) / 2, y, s);
  if (prev) g.setFont(prev);
}
void text_center(U8G2& g, int y, const char* s) {
  g.drawUTF8(42 - (int)g.getUTF8Width(s) / 2, y, s);
}
int bold_width(U8G2& g, const char* s) {
  const uint8_t* prev = g.getU8g2()->font;
  g.setFont(u8g2_font_3310_small_bold);
  int w = (int)g.getUTF8Width(s);
  if (prev) g.setFont(prev);
  return w;
}
void softkey(U8G2& g, const char* label) { text_bold_center(g, 47, label); }
void inv_str(U8G2& g, int x, int baseline, const char* s) {
  g.drawBox(x - 1, baseline - 7, (int)g.getStrWidth(s) + 2, 9);
  g.setDrawColor(0);
  g.drawStr(x, baseline, s);
  g.setDrawColor(1);
}
void poda(U8G2& g, char* s, int max_w) {
  if ((int)g.getUTF8Width(s) <= max_w) return;
  size_t n = strlen(s);
  while (n > 1) {
    do { n--; } while (n > 1 && (s[n] & 0xC0) == 0x80);  // char UTF-8 inteiro
    s[n] = '.';
    s[n + 1] = '\0';
    if ((int)g.getUTF8Width(s) <= max_w) return;
  }
}
void no_network(U8G2& g) {
  g.drawXBMP((84 - ICON_NOWIFI_W) / 2, 11, ICON_NOWIFI_W, ICON_NOWIFI_H,
             icon_nowifi_bits);
  text_center(g, 38, tr(STR_CONNECT_WIFI));
}
}  // namespace nokia_ui
