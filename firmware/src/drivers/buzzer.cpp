#include "buzzer.h"
#include <Arduino.h>
#include "pins.h"
#include "rtttl.h"

namespace buzzer {
static const uint8_t kCh = 0;
static Rtttl rtttl_;
static bool playing_ = false;       // tune em andamento
static uint32_t tone_end_ = 0;      // fim do som da nota (90% — gap audível)
static uint32_t note_end_ = 0;      // fim do slot da nota (100% — busca a próxima)
static bool tone_on_ = false;

// comparação imune ao rollover de millis()
static inline bool reached(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }

void init() {
  ledcSetup(kCh, 2000, 10);
  ledcAttachPin(PIN_BUZZER, kCh);
  ledcWriteTone(kCh, 0);
}
static void tone_for(uint16_t f, uint16_t tone_ms, uint16_t note_ms, uint32_t now) {
  ledcWriteTone(kCh, f);
  tone_on_ = true;
  tone_end_ = now + tone_ms;
  note_end_ = now + note_ms;
}
void beep(uint16_t f, uint16_t ms) { playing_ = false; tone_for(f, ms, ms, millis()); }
void play(const char* tune) {
  stop();
  playing_ = rtttl_.begin(tune);
  if (playing_) { uint32_t now = millis(); tone_end_ = now; note_end_ = now; }
}
void stop() { playing_ = false; tone_on_ = false; ledcWriteTone(kCh, 0); }
bool busy() { return playing_ || tone_on_; }

void tick(uint32_t now) {
  if (tone_on_ && reached(now, tone_end_)) { ledcWriteTone(kCh, 0); tone_on_ = false; }
  if (playing_ && reached(now, note_end_)) {
    RtttlNote n;
    if (rtttl_.next(n)) {
      tone_for(n.freq_hz, (uint16_t)(n.dur_ms * 9 / 10), n.dur_ms, now);  // 10% gap
    } else playing_ = false;
  }
}
}  // namespace buzzer
