#include "boot_anim.h"
#include <Arduino.h>
#include "assets.h"
#include "drivers/buzzer.h"

// startup chime dos Nokia com as maos: arpejo ascendente de La maior
// terminando em A sustentado (aproximacao monofonica — afinar na bancada)
static const char* kStartupChime =
    "Startup:d=8,o=5,b=140:e,a,c#6,e6,2a6";

// espera dur_ms mantendo o chime andando
static void wait_ticking(uint32_t dur_ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < dur_ms) { buzzer::tick(millis()); delay(2); }
}

void boot_anim_play(U8G2& g) {
  buzzer::play(kStartupChime);
  // maos entram pelas laterais e convergem; a de cima (right) com dedos pra
  // esquerda, a de baixo (left) com dedos pra direita, pontas se cruzando
  const int kFrames = 12;
  const int lx0 = -HAND_LEFT_W, lx1 = 18;   // mao de baixo: entra pela esquerda
  const int rx0 = 84, rx1 = 38;             // mao de cima: entra pela direita
  for (int f = 0; f <= kFrames; f++) {
    g.clearBuffer();
    g.drawXBMP(19, 6, NOKIA_LOGO_W, NOKIA_LOGO_H, nokia_logo_bits);
    g.drawXBMP(rx0 + (rx1 - rx0) * f / kFrames, 18, HAND_RIGHT_W, HAND_RIGHT_H,
               hand_right_bits);
    g.drawXBMP(lx0 + (lx1 - lx0) * f / kFrames, 30, HAND_LEFT_W, HAND_LEFT_H,
               hand_left_bits);
    g.sendBuffer();
    wait_ticking(70);
  }
  // segura logo + maos juntas ate o chime acabar
  uint32_t t0 = millis();
  while (buzzer::busy() && millis() - t0 < 4000) { buzzer::tick(millis()); delay(2); }
}
