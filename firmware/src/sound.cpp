#include "sound.h"
#include <Preferences.h>
#include "apps/app_tones.h"
#include "drivers/buzzer.h"

namespace sound {

static Preferences prefs_;
static bool loaded_ = false;
static uint8_t vol_ = 2;   // 3310 saia de fabrica no talo
static uint8_t tone_ = 0;  // Nokia tune
static bool mute_ = false;

static void load() {
  if (loaded_) return;
  prefs_.begin("espnokia");
  vol_ = prefs_.getUChar("vol", 2);
  tone_ = prefs_.getUChar("tone", 0);
  mute_ = prefs_.getBool("mute", false);
  if (vol_ > 2) vol_ = 2;
  if (tone_ >= tones_count()) tone_ = 0;
  loaded_ = true;
}

void init() {
  load();
  buzzer::set_volume(vol_);
  buzzer::set_mute(mute_);
}

// bips multi-nota como mini-RTTTL: ganham o gap e o timing do player de
// toques; so o de tecla e um beep cru, pra nao interromper musica tocando
void play(Snd s) {
  switch (s) {
    case SND_KEY: buzzer::beep(900, 90); break;  // medido de um 3310 real
    case SND_CONFIRM: buzzer::play("ok:d=16,o=6,b=200:c,e"); break;
    case SND_ERROR: buzzer::play("err:d=8,o=4,b=160:e,c"); break;
    case SND_NOTIFY: buzzer::play("sms:d=16,o=6,b=160:g,p,g"); break;
    case SND_ALERT: buzzer::play("alr:d=16,o=6,b=180:c,e,g,p,c,e,g"); break;
  }
}

void ringtone() {
  load();
  buzzer::play(tones_rtttl(tone_));
}

uint8_t volume() {
  load();
  return vol_;
}
void set_volume(uint8_t lvl) {
  load();
  vol_ = lvl > 2 ? 2 : lvl;
  buzzer::set_volume(vol_);
  prefs_.putUChar("vol", vol_);
}

bool muted() { load(); return mute_; }
void set_muted(bool m) {
  load();
  mute_ = m;
  buzzer::set_mute(m);
  prefs_.putBool("mute", m);
}

uint8_t ringtone_idx() {
  load();
  return tone_;
}
void set_ringtone(uint8_t idx) {
  load();
  tone_ = idx < tones_count() ? idx : 0;
  prefs_.putUChar("tone", tone_);
}

}  // namespace sound
