#include "app_about.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "version.h"
#include "ui/assets.h"
#include "ui/nokia_ui.h"

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char buf[24];
  g.setFont(u8g2_font_nokiafc22_tr);
  nokia_ui::text_bold_center(g, 8, "Sobre");
  g.drawStr(2, 19, "espnokia-phone");
  snprintf(buf, sizeof(buf), "fw %s", ESPNOKIA_FW_VERSION);
  g.drawStr(2, 28, buf);
  snprintf(buf, sizeof(buf), "heap %u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  g.drawStr(2, 37, buf);
  nokia_ui::softkey(g, "Voltar");
}
const App app_about = {"Sobre", nullptr, nullptr, nullptr, nullptr, render,
                       icon_about_bits};
