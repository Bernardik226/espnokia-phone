#include "app_tones.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "drivers/buzzer.h"
#include "i18n.h"
#include "sound.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// toques originais de fabrica da Nokia (era 3310/1100), transcricoes RTTTL
struct Tone { const char* name; const char* rtttl; };
static const Tone kTones[] = {
    {"Nokia tune", "Nokia:d=4,o=5,b=225:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a"},
    {"Ring ring", "RingRing:d=4,o=5,b=100:32b,32d6,32g6,32g6,32g6,8p,32b,32d6,"
                  "32g6,32g6,32g6,2p,32b,32d6,32g6,32g6,32g6,8p,32b,32d6,32g6,"
                  "32g6,32g6,2p,32b,32d6,32g6,32g6,32g6,8p,32b,32d6,32g6,32g6,32g6"},
    {"City Bird", "CityBird:d=4,o=5,b=125:16e6,16e6,8d.6,16e6,8d.6,16e6,d6,16e6,"
                  "16e6,8d.6,16e6,8d.6,16e6,d6,16e6,16e6,8d.6,16e6,8d.6,16e6,d6,"
                  "16e6,16e6,8d.6,16e6,8d.6,16e6,d6"},
    {"Groovy Blue", "GroovyBl:d=32,o=6,b=112:p,16g,16a#,16g,a#.,16f.,a,8p.,a,8p.,"
                    "a,8p,a,8p,a,8p.,a,a#.,a.,a,8a#,a,8g#,a,8p.,a,8p.,a,8p.,a,"
                    "8p.,a,8p.,a,8p,16g,16a#,16g,16a#,16f,a,8p.,a,8p.,a,8p,a,8p,"
                    "a,8p."},
    {"Entertainer", "Entertai:d=16,o=5,b=140:8d6,8d#6,8e6,4c7,8e6,4c7,8e6,2c7,"
                    "8c7,8d7,8d#7,8e7,8c7,8d7,4e7,8b6,4d7,2c7,4p,8d6,8d#6,8e6,"
                    "4c7,8e6,4c7,8e6,2c7,8p,8a6,8g6,8f#6,8a6,8c7,4e7,8d7,8c7,"
                    "8a6,2d7"},
    {"Toreador", "Toreador:d=16,o=5,b=112:8c6.,8d6.,32c6.,4a,4a,8a.,32g.,8a.,"
                 "32a#.,4a.,p.,8a#.,8g.,32c6.,2a,8f.,8d.,32g.,2c,4g.,g.,d6.,c6.,"
                 "a#.,a.,g.,a.,a#.,2a,8e.,4a,8a.,8g#.,32b.,4e6.,2e6,d6.,c#6.,"
                 "d6.,g.,a.,4a#,a.,f.,d6.,2c6,f.,c.,a#.,8a.,8g.,4f."},
    {"Minuet", "Minuet:d=16,o=5,b=125:4d6,8g,8a,8b,8c6,4d6,4g,4g,4e6,8c6,8d6,"
               "8e6,8f6,4g6,4g,4g,4c6,8d6,8c6,8b,8a,4b,8c6,8b,8a,8g,4f#,8g,8a,"
               "8b,8g,4b,4a"},
    {"Circles", "Circles:d=16,o=5,b=180:a6,a,c6,e6,8a6,8a,8c7,8b6,8a6,8f.6,f,a,"
                "c6,8f6,8f,8a6,8g6,8f6,8g.6,g,b,d6,8g6,8g,8c7,8b6,8a6,4a6,2p,f,"
                "a,c6,f,f6,p,g,b,d6,g,g6,2p"},
    {"Auld L Syne", "AuldLang:d=32,o=6,b=120:4d,4g.,8g,4g,4b,4a.,8g,4a,4b,4g.,"
                    "8g,4b,4d7,2e7,4p,4e7,4d.7,8b,4b,4g,4a.,8g,4a,4b,4g.,8e,4e,"
                    "4d,2g,4p,4e7,4d.7,8b,4b,4g,4a.,8g,4a,4e7,4d.7,8b,4b,4d7,"
                    "2e7,4p,4g7,4d.7,8b,4b,4g,4a.,8g,4a,4b,4g.,8e,4e,4d,2g,4p"},
};
static const uint8_t kCount = sizeof(kTones) / sizeof(kTones[0]);
static const uint8_t kVisible = 3;  // linhas na janela (header + 3 + softkey)
static uint8_t cur = 0, top = 0;
static uint32_t preview_at_ = 0;  // 0 = nenhum preview agendado

static void clamp_window() {
  if (cur < top) top = cur;
  if (cur >= top + kVisible) top = cur - (kVisible - 1);
}

// UX do 3310 em "Toque de chamada": navegar da o preview do toque (com um
// respiro pra rolagem rapida nao tocar tudo), OK define como padrao do sistema
static bool input(Button b, BtnEvent e) {
  if (e != EV_PRESS) return false;
  if (b == BTN_UP) { cur = (cur + kCount - 1) % kCount; clamp_window(); }
  else if (b == BTN_DOWN) { cur = (cur + 1) % kCount; clamp_window(); }
  else if (b == BTN_OK) {
    sound::set_ringtone(cur);
    sound::play(sound::SND_CONFIRM);  // tambem cala o preview
    preview_at_ = 0;
    return true;
  } else {
    return false;  // C nao consumido → shell volta pro menu
  }
  buzzer::stop();
  preview_at_ = millis() + 500;
  return true;
}
static void on_enter() { preview_at_ = 0; }
static void tick(uint32_t now) {
  if (preview_at_ && (int32_t)(now - preview_at_) >= 0) {
    preview_at_ = 0;
    buzzer::play(kTones[cur].rtttl);
  }
}
static void on_exit() { buzzer::stop(); }
static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_3310_small);
  nokia_ui::text_bold_center(g, 8, tr(STR_APP_TONES));
  for (uint8_t row = 0; row < kVisible && top + row < kCount; row++) {
    uint8_t i = top + row;                          // lista com barra invertida (3310)
    int y = 11 + row * 9;
    bool sel = (i == cur);
    nokia_ui::list_row(g, y, 80, kTones[i].name, sel);
    if (i == sound::ringtone_idx()) {               // check no toque padrao
      if (sel) g.setDrawColor(0);
      g.drawLine(72, y + 5, 73, y + 7);
      g.drawLine(73, y + 7, 76, y + 3);
      if (sel) g.setDrawColor(1);
    }
  }
  g.drawVLine(82, 11, 27);                          // scrollbar como no menu
  int th = 27 * kVisible / kCount;
  g.drawBox(81, 11 + (27 - th) * top / (kCount - kVisible), 3, th);
  nokia_ui::softkey(g, tr(STR_SELECT));
}
uint8_t tones_count() { return kCount; }
const char* tones_name(uint8_t i) { return kTones[i % kCount].name; }
const char* tones_rtttl(uint8_t i) { return kTones[i % kCount].rtttl; }

// animacao de selecao: a nota danca (sobe, desce, sobe) e assenta
static const unsigned char* const kAnim[] = {
    icon_tones_f1_bits, icon_tones_f2_bits, icon_tones_f1_bits, nullptr};
const App app_tones = {STR_APP_TONES, on_enter, tick, input, on_exit, render,
                       icon_tones_bits, kAnim};
