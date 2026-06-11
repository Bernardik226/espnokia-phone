#include "app_about.h"
#include <Arduino.h>
#include <U8g2lib.h>

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char buf[24];
  g.setFont(u8g2_font_profont11_tr);
  g.drawStr(3, 18, "espnokia-phone");
  g.setFont(u8g2_font_4x6_tr);
  g.drawStr(3, 28, "fw F1 0.1.0");
  snprintf(buf, sizeof(buf), "heap %u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  g.drawStr(3, 36, buf);
  snprintf(buf, sizeof(buf), "up %lu s", (unsigned long)(millis() / 1000));
  g.drawStr(3, 44, buf);
}
const App app_about = {"Sobre", nullptr, nullptr, nullptr, nullptr, render};
