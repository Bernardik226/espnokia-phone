#include "alarm.h"
#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include "drivers/buzzer.h"
#include "i18n.h"
#include "sound.h"
#include "drivers/rtc.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

namespace alarme {

static Preferences prefs_;
static bool loaded_ = false;
static bool armed_ = false;
static uint8_t d_, mo_, h_, mi_;
static char label_[16];
static bool ringing_ = false;

static void load() {
  if (loaded_) return;
  prefs_.begin("espnokia");
  armed_ = prefs_.getBool("al_on", false);
  d_ = prefs_.getUChar("al_d", 0);
  mo_ = prefs_.getUChar("al_mo", 0);
  h_ = prefs_.getUChar("al_h", 0);
  mi_ = prefs_.getUChar("al_mi", 0);
  prefs_.getString("al_lbl", label_, sizeof(label_));
  loaded_ = true;
}

void init() { load(); }

void set(uint8_t dia, uint8_t mes, uint8_t h, uint8_t m, const char* label) {
  load();
  armed_ = true;
  d_ = dia; mo_ = mes; h_ = h; mi_ = m;
  snprintf(label_, sizeof(label_), "%s", label);
  prefs_.putBool("al_on", true);
  prefs_.putUChar("al_d", d_);
  prefs_.putUChar("al_mo", mo_);
  prefs_.putUChar("al_h", h_);
  prefs_.putUChar("al_mi", mi_);
  prefs_.putString("al_lbl", label_);
}

void clear() {
  load();
  armed_ = false;
  prefs_.putBool("al_on", false);
}

bool armed(uint8_t dia, uint8_t mes, uint8_t h, uint8_t m) {
  load();
  return armed_ && dia == d_ && mes == mo_ && h == h_ && m == mi_;
}

bool active() { return ringing_; }

// "minuto do ano" aproximado: suficiente pra janela de 5 min no mesmo dia
static int32_t minuto(uint8_t mes, uint8_t dia, uint8_t h, uint8_t m) {
  return (((int32_t)mes * 31 + dia) * 24 + h) * 60 + m;
}

void tick(uint32_t) {
  if (ringing_) {
    if (!buzzer::tune_busy()) sound::ringtone();  // loop no toque padrao
    return;
  }
  if (!armed_) return;
  rtc::DateTime dt;
  if (!rtc::now(dt)) return;  // rtc::now cacheia 1 s, barato por loop
  int32_t delta = minuto(dt.month, dt.day, dt.hour, dt.min) - minuto(mo_, d_, h_, mi_);
  if (delta < 0 || delta > 5) return;  // janela de 5 min (tolera reboot)
  ringing_ = true;
  armed_ = false;
  prefs_.putBool("al_on", false);  // one-shot
  sound::ringtone();
}

bool input(Button b, BtnEvent e) {
  if (!ringing_) return false;
  if (e == EV_PRESS && (b == BTN_OK || b == BTN_C)) {
    ringing_ = false;
    buzzer::stop();
  }
  return true;  // engole tudo enquanto o overlay esta na tela
}

void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setDrawColor(0);
  g.drawBox(0, 0, 84, 48);  // apaga o app por baixo: overlay fullscreen
  g.setDrawColor(1);
  g.setFont(u8g2_font_3310_small);
  bool pisca = ((millis() / 400) % 2) == 0;  // urgencia, estilo alarme 3310
  if (pisca) nokia_ui::text_bold_center(g, 14, tr(STR_GAME_NOW));
  g.drawStr(42 - (int)g.getStrWidth(label_) / 2, 27, label_);
  nokia_ui::softkey(g, tr(STR_OK));
}

}  // namespace alarme
