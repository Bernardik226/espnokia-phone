#include "goal_fx.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "drivers/buzzer.h"
#include "i18n.h"
#include "sound.h"
#include "assets.h"
#include "fonts3310.h"
#include "nokia_ui.h"

namespace goal_fx {

static const uint32_t kRollMs = 1200;   // travessia da bola
static const uint32_t kTotalMs = 6000;  // efeito inteiro (bola + GOL!)

static bool active_ = false;
static bool diag_ = false;        // sorteio: horizontal ou diagonal
static uint32_t t0_ = 0;
static char placar_[20];
static char autor_[32];

// dedup: o mesmo gol pode ser visto pelo detail aberto E pelo vigia
static char last_placar_[20];
static uint32_t last_ms_ = 0;

static void stop_fx() {
  active_ = false;
  buzzer::stop();  // o toque so vive durante o efeito
}

void start(const char* placar, const char* autor) {
  uint32_t now = millis();
  if (active_) return;
  if (strcmp(placar, last_placar_) == 0 && now - last_ms_ < 10 * 60000)
    return;  // este placar acabou de aparecer na tela
  snprintf(placar_, sizeof(placar_), "%s", placar);
  snprintf(autor_, sizeof(autor_), "%s", autor ? autor : "");
  snprintf(last_placar_, sizeof(last_placar_), "%s", placar);
  last_ms_ = now;
  t0_ = now;
  diag_ = esp_random() & 1;
  active_ = true;
  sound::ringtone();
}

bool active() { return active_; }

void tick(uint32_t now) {
  if (!active_) return;
  if (!buzzer::tune_busy()) sound::ringtone();  // loop no toque padrao
  if (now - t0_ >= kTotalMs) stop_fx();
}

bool input(Button b, BtnEvent e) {
  if (!active_) return false;
  if (e == EV_PRESS && (b == BTN_OK || b == BTN_C)) stop_fx();
  return true;  // engole tudo enquanto o efeito esta na tela
}

void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setDrawColor(0);
  g.drawBox(0, 0, 84, 48);  // apaga o app por baixo: overlay fullscreen
  g.setDrawColor(1);
  uint32_t dt = millis() - t0_;
  if (dt < kRollMs) {
    // bola girando (frame novo a cada 80 ms) cruzando a tela
    static const unsigned char* const kBall[] = {ball_f0_bits, ball_f1_bits,
                                                 ball_f2_bits, ball_f3_bits};
    int x = -16 + (int)(dt * 100 / kRollMs);
    int y = diag_ ? -16 + (int)(dt * 64 / kRollMs) : 16;
    g.drawXBMP(x, y, BALL_F0_W, BALL_F0_H, kBall[(dt / 80) % 4]);
    return;
  }
  g.setFont(u8g2_font_3310_small);
  if ((millis() / 250) % 2 == 0)  // urgencia, mesmo pisca do alarme
    nokia_ui::text_bold_center(g, 14, tr(STR_GOAL));
  nokia_ui::text_bold_center(g, 27, placar_);
  if (autor_[0])
    nokia_ui::text_center(g, 38, autor_);
}

}  // namespace goal_fx
