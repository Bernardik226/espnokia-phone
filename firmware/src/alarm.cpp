#include "alarm.h"
#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include "drivers/buzzer.h"
#include "i18n.h"
#include "drivers/rtc.h"
#include "sound.h"
#include "timeutil.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

namespace alarme {

static Preferences prefs_;
static bool loaded_ = false;
static bool armed_ = false;
static uint8_t d_, mo_, h_, mi_;
static StrId title_ = STR_GAME_NOW;     // titulo do overlay ao disparar
static char label_[16];
static bool ringing_ = false;
static StrId ring_title_ = STR_GAME_NOW;
static char ring_label_[16];

// timer regressivo: volatil de proposito (reboot cancela, como nos fornos)
static bool timer_on_ = false;
static uint32_t timer_end_ = 0;
static uint16_t timer_min_ = 0;

static void load() {
  if (loaded_) return;
  prefs_.begin("espnokia");
  armed_ = prefs_.getBool("al_on", false);
  d_ = prefs_.getUChar("al_d", 0);
  mo_ = prefs_.getUChar("al_mo", 0);
  h_ = prefs_.getUChar("al_h", 0);
  mi_ = prefs_.getUChar("al_mi", 0);
  title_ = (StrId)prefs_.getUChar("al_tit", STR_GAME_NOW);
  if (title_ >= STR_COUNT) title_ = STR_GAME_NOW;
  prefs_.getString("al_lbl", label_, sizeof(label_));
  loaded_ = true;
}

void init() { load(); }

void set(StrId title, uint8_t dia, uint8_t mes, uint8_t h, uint8_t m,
         const char* label) {
  load();
  armed_ = true;
  title_ = title;
  d_ = dia; mo_ = mes; h_ = h; mi_ = m;
  snprintf(label_, sizeof(label_), "%s", label);
  prefs_.putBool("al_on", true);
  prefs_.putUChar("al_d", d_);
  prefs_.putUChar("al_mo", mo_);
  prefs_.putUChar("al_h", h_);
  prefs_.putUChar("al_mi", mi_);
  prefs_.putUChar("al_tit", (uint8_t)title_);
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

bool armed_at(uint8_t& h, uint8_t& m) {
  load();
  h = h_; m = mi_;
  return armed_;
}

void trigger(StrId title, const char* label) {
  ring_title_ = title;
  snprintf(ring_label_, sizeof(ring_label_), "%s", label);
  ringing_ = true;
  sound::ringtone();
}

void timer_start(uint16_t minutos) {
  timer_min_ = minutos;
  timer_end_ = millis() + (uint32_t)minutos * 60000;
  timer_on_ = true;
}
void timer_cancel() { timer_on_ = false; }
uint32_t timer_left_s() {
  if (!timer_on_) return 0;
  int32_t left = (int32_t)(timer_end_ - millis());
  return left > 0 ? (uint32_t)(left + 999) / 1000 : 0;
}

bool active() { return ringing_; }

void tick(uint32_t now) {
  if (ringing_) {
    if (!buzzer::tune_busy()) sound::ringtone();  // loop no toque padrao
    return;
  }
  if (timer_on_ && timeutil::reached(now, timer_end_)) {
    timer_on_ = false;
    char lbl[16];
    snprintf(lbl, sizeof(lbl), "%u min", timer_min_);
    trigger(STR_TIME_UP, lbl);
    return;
  }
  if (!armed_) return;
  rtc::DateTime dt;
  if (!rtc::now(dt)) return;  // rtc::now cacheia 1 s, barato por loop
  int32_t delta = timeutil::minuto(dt.month, dt.day, dt.hour, dt.min) -
                  timeutil::minuto(mo_, d_, h_, mi_);
  if (delta < 0 || delta > 5) return;  // janela de 5 min (tolera reboot)
  armed_ = false;
  prefs_.putBool("al_on", false);  // one-shot
  trigger(title_, label_);
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
  if (pisca) nokia_ui::text_bold_center(g, 14, tr(ring_title_));
  g.drawStr(42 - (int)g.getStrWidth(ring_label_) / 2, 27, ring_label_);
  nokia_ui::softkey(g, tr(STR_OK));
}

}  // namespace alarme
