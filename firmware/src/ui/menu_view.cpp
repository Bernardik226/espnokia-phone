#include "menu_view.h"
#include "assets.h"
#include "fonts3310.h"
#include "i18n.h"
#include "nokia_ui.h"

static const uint16_t kFrameMs = 90;  // animacao de selecao: ~3 frames/270ms

void menu_view_draw(U8G2& g, const App& a, uint8_t n, uint8_t cur) {
  g.setFont(u8g2_font_3310_small);
  // header: nome do app (traduzido) bold no topo
  nokia_ui::text_bold_center(g, 8, tr((StrId)a.name_id));
  // animacao de selecao: item novo sob o cursor -> toca os frames uma vez
  // e descansa no icone (o loop ja redesenha a ~20 fps, basta escolher o
  // frame pelo relogio)
  static const App* ult = nullptr;
  static uint32_t t0 = 0;
  uint32_t now = millis();
  if (&a != ult) {
    ult = &a;
    t0 = now;
  }
  const unsigned char* img = a.icon;
  if (a.anim) {
    uint32_t i = (now - t0) / kFrameMs;
    const unsigned char* const* f = a.anim;
    while (*f && i) {  // anda i frames; para no null
      f++;
      i--;
    }
    if (*f) img = *f;  // sequencia acabou -> fica no icone de descanso
  }
  // icone 28x24 central (alargado pra compensar o pixel retangular do 5110;
  // area util desconta a scrollbar a direita)
  if (img) g.drawXBMP(26, 11, ICON_CLOCK_W, ICON_CLOCK_H, img);
  // scrollbar proporcional na lateral direita (trilho + thumb)
  const int track_y = 11, track_h = 26;
  g.drawVLine(82, track_y, track_h);
  int th = track_h / (n > 0 ? n : 1);
  if (th < 6) th = 6;
  int ty = (n > 1) ? track_y + (track_h - th) * cur / (n - 1) : track_y;
  g.drawBox(81, ty, 3, th);
  nokia_ui::softkey(g, tr(STR_SELECT));
}

void menu_view_draw(U8G2& g, const Shell& shell) {
  menu_view_draw(g, *shell.app_at(shell.cursor()), shell.app_count(),
                 shell.cursor());
}
