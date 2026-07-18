#include "buzzer.h"
#include <Arduino.h>
#include "pins.h"
#include "rtttl.h"
#include "timeutil.h"

namespace buzzer {
static const uint8_t kCh = 0;
// volume = energia do pulso (10 bits de resolucao). Medido no piezo real:
// ~9% de duty soa MAIS ALTO que 50% — as harmonicas do pulso curto caem na
// ressonancia da capsula. A escada e por loudness ouvida, nao por duty.
static const uint16_t kDuty[] = {12, 512, 96};
static uint8_t vol_ = 2;
static bool muted_ = false;         // mute geral do sistema
static Rtttl rtttl_;
static bool playing_ = false;       // tune em andamento
static uint32_t tone_end_ = 0;      // fim do som da nota (90% — gap audível)
static uint32_t note_end_ = 0;      // fim do slot da nota (100% — busca a próxima)
static bool tone_on_ = false;

void init() {
  ledcSetup(kCh, 2000, 10);
  ledcAttachPin(PIN_BUZZER, kCh);
  ledcWriteTone(kCh, 0);
}
static void tone_for(uint16_t f, uint16_t tone_ms, uint16_t note_ms, uint32_t now) {
  ledcWriteTone(kCh, f);                 // reseta o duty pra 50%...
  if (f) ledcWrite(kCh, kDuty[vol_]);    // ...entao o volume reaplica por nota
  tone_on_ = true;
  tone_end_ = now + tone_ms;
  note_end_ = now + note_ms;
}
void set_volume(uint8_t lvl) { vol_ = lvl > 2 ? 2 : lvl; }
void set_mute(bool m) { muted_ = m; if (m) stop(); }
bool muted() { return muted_; }
// um piezo, um canal: o tune tem prioridade (fiel ao 3310 — tecla muda durante toque)
void beep(uint16_t f, uint16_t ms) {
  if (playing_ || muted_) return;
  tone_for(f, ms, ms, millis());
}
void play(const char* tune) {
  if (muted_) return;
  stop();
  playing_ = rtttl_.begin(tune);
  if (playing_) { uint32_t now = millis(); tone_end_ = now; note_end_ = now; }
}
void stop() { playing_ = false; tone_on_ = false; ledcWriteTone(kCh, 0); }
bool busy() { return playing_ || tone_on_; }
bool tune_busy() { return playing_; }

void tick(uint32_t now) {
  if (tone_on_ && timeutil::reached(now, tone_end_)) { ledcWriteTone(kCh, 0); tone_on_ = false; }
  if (playing_ && timeutil::reached(now, note_end_)) {
    RtttlNote n;
    if (rtttl_.next(n)) {
      tone_for(n.freq_hz, (uint16_t)(n.dur_ms * 9 / 10), n.dur_ms, now);  // 10% gap
    } else playing_ = false;
  }
}
}  // namespace buzzer
