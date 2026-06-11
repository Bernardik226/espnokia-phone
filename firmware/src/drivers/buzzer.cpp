#include "buzzer.h"
#include <Arduino.h>
#include "pins.h"
#include "rtttl.h"

namespace buzzer {
static const uint8_t kCh = 0;
static Rtttl rtttl_;
static bool playing_ = false;       // tune em andamento
static uint32_t note_end_ = 0;      // fim da nota/bip atual
static bool tone_on_ = false;

void init() {
  ledcSetup(kCh, 2000, 10);
  ledcAttachPin(PIN_BUZZER, kCh);
  ledcWriteTone(kCh, 0);
}
static void tone_for(uint16_t f, uint16_t ms, uint32_t now) {
  ledcWriteTone(kCh, f);
  tone_on_ = true;
  note_end_ = now + ms;
}
void beep(uint16_t f, uint16_t ms) { playing_ = false; tone_for(f, ms, millis()); }
void play(const char* tune) { playing_ = rtttl_.begin(tune); if (playing_) note_end_ = 0; }
void stop() { playing_ = false; tone_on_ = false; ledcWriteTone(kCh, 0); }
bool busy() { return playing_ || tone_on_; }

void tick(uint32_t now) {
  if (tone_on_ && now >= note_end_) { ledcWriteTone(kCh, 0); tone_on_ = false; }
  if (playing_ && now >= note_end_) {
    RtttlNote n;
    if (rtttl_.next(n)) {
      tone_for(n.freq_hz, (uint16_t)(n.dur_ms * 9 / 10), now);  // 10% gap entre notas
      note_end_ = now + n.dur_ms;
      playing_ = true;
    } else playing_ = false;
  }
}
}  // namespace buzzer
