#include "app_tones.h"
#include <U8g2lib.h>
#include "drivers/buzzer.h"

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
  if (b == BTN_OK) { buzzer::play(kTones[cur].rtttl); return true; }
  return false;  // C não consumido → shell volta pro launcher
}
static void on_exit() { buzzer::stop(); }
static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_profont11_tr);
  g.drawStr(3, 18, "Toque:");
  g.drawBox(0, 22, 84, 11);
  g.setDrawColor(0);
  g.drawStr(6, 31, kTones[cur].name);
  g.setDrawColor(1);
  g.setFont(u8g2_font_4x6_tr);
  g.drawStr(3, 45, buzzer::busy() ? "tocando..." : "OK toca  C volta");
}
const App app_tones = {"Toques", nullptr, nullptr, input, on_exit, render};
