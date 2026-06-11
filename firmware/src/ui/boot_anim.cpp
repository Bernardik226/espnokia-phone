#include "boot_anim.h"
#include <Arduino.h>
#include "assets.h"
#include "drivers/buzzer.h"

// startup chime dos Nokia mono (1100/2300): as 5 notas finais do Nokia tune,
// B-A-C#-E-A em La maior, nota final pontuada e sustentada (~880 Hz no piezo)
static const char* kStartupChime =
    "Startup:d=4,o=5,b=125:8b,8a,c#,e,2a.";

// espera dur_ms mantendo o chime andando
static void wait_ticking(uint32_t dur_ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < dur_ms) { buzzer::tick(millis()); delay(2); }
}

void boot_anim_play(U8G2& g) {
  buzzer::play(kStartupChime);
  // tela de boot do 1100: mao adulta a esquerda, mao de bebe a direita,
  // logo NOKIA embaixo; as maos entram pelas laterais e convergem
  const int kFrames = 12;
  const int lx0 = -HAND_LEFT_W, lx1 = 0;    // mao adulta: entra pela esquerda
  const int rx0 = 84, rx1 = 51;             // mao de bebe: entra pela direita
  for (int f = 0; f <= kFrames; f++) {
    g.clearBuffer();
    g.drawXBMP(6, 39, NOKIA_LOGO_W, NOKIA_LOGO_H, nokia_logo_bits);
    g.drawXBMP(rx0 + (rx1 - rx0) * f / kFrames, 17, HAND_RIGHT_W, HAND_RIGHT_H,
               hand_right_bits);
    g.drawXBMP(lx0 + (lx1 - lx0) * f / kFrames, 0, HAND_LEFT_W, HAND_LEFT_H,
               hand_left_bits);
    g.sendBuffer();
    wait_ticking(70);
  }
  // segura logo + maos juntas ate o chime acabar
  uint32_t t0 = millis();
  while (buzzer::busy() && millis() - t0 < 4000) { buzzer::tick(millis()); delay(2); }
}
