#include "menu_view.h"
#include "assets.h"
#include "fonts3310.h"
#include "i18n.h"
#include "nokia_ui.h"

void menu_view_draw(U8G2& g, const Shell& shell) {
  const App* a = shell.app_at(shell.cursor());
  g.setFont(u8g2_font_3310_small);
  // header: nome do app (traduzido) bold no topo
  nokia_ui::text_bold_center(g, 8, tr((StrId)a->name_id));
  // icone 28x24 central (alargado pra compensar o pixel retangular do 5110;
  // area util desconta a scrollbar a direita)
  if (a->icon) g.drawXBMP(26, 11, ICON_CLOCK_W, ICON_CLOCK_H, a->icon);
  // scrollbar proporcional na lateral direita (trilho + thumb)
  const int track_y = 11, track_h = 26;
  g.drawVLine(82, track_y, track_h);
  int n = shell.app_count();
  int th = track_h / (n > 0 ? n : 1);
  if (th < 6) th = 6;
  int ty = (n > 1) ? track_y + (track_h - th) * shell.cursor() / (n - 1) : track_y;
  g.drawBox(81, ty, 3, th);
  nokia_ui::softkey(g, tr(STR_SELECT));
}
