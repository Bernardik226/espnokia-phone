#include "launcher_view.h"

void launcher_view_draw(U8G2& g, const Shell& shell) {
  g.setFont(u8g2_font_profont11_tr);
  const uint8_t kRow = 10, kTop = 10;
  uint8_t first = (shell.cursor() / 4) * 4;  // paginação de 4 em 4
  for (uint8_t i = 0; i < 4 && first + i < shell.app_count(); i++) {
    uint8_t idx = first + i;
    int y = kTop + i * kRow;
    if (idx == shell.cursor()) {
      g.drawBox(0, y, 84, kRow);
      g.setDrawColor(0);
      g.drawStr(3, y + 8, shell.app_at(idx)->name);
      g.setDrawColor(1);
    } else {
      g.drawStr(3, y + 8, shell.app_at(idx)->name);
    }
  }
}
