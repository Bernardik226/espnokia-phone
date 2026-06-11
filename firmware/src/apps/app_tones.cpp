#include "app_tones.h"
#include <U8g2lib.h>
#include "drivers/buzzer.h"
#include "ui/assets.h"
#include "ui/nokia_ui.h"

struct Tone { const char* name; const char* rtttl; };
static const Tone kTones[] = {
    {"Nokia", "Nokia:d=4,o=5,b=225:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a"},
    {"Kuckuck", "Kuckuck:d=4,o=5,b=125:8g,8e,8g,8e,8g,8e,8c6,8a,8g,8e,8f,8d,2c"},
    {"Sweep", "Sweep:d=16,o=5,b=140:c,e,g,c6,g,e,c"},
};
static const uint8_t kCount = sizeof(kTones) / sizeof(kTones[0]);
static uint8_t cur = 0;

static bool input(Button b, BtnEvent e) {
  if (e != EV_PRESS) return false;
  if (b == BTN_UP) { cur = (cur + kCount - 1) % kCount; return true; }
  if (b == BTN_DOWN) { cur = (cur + 1) % kCount; return true; }
  if (b == BTN_OK) {  // toggle: tocando para, parado toca (tune_busy ignora o beep de tecla)
    if (buzzer::tune_busy()) buzzer::stop();
    else buzzer::play(kTones[cur].rtttl);
    return true;
  }
  return false;  // C nao consumido → shell volta pro menu
}
static void on_exit() { buzzer::stop(); }
static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_nokiafc22_tr);
  nokia_ui::text_bold_center(g, 8, "Toques");
  for (uint8_t i = 0; i < kCount; i++) {            // lista com barra invertida (3310)
    int y = 11 + i * 9;
    if (i == cur) {
      g.drawBox(0, y, 84, 9);
      g.setDrawColor(0);
      g.drawStr(3, y + 7, kTones[i].name);
      g.setDrawColor(1);
    } else {
      g.drawStr(3, y + 7, kTones[i].name);
    }
  }
  nokia_ui::softkey(g, buzzer::tune_busy() ? "Parar" : "Tocar");
}
const App app_tones = {"Toques", nullptr, nullptr, input, on_exit, render,
                       icon_tones_bits};
