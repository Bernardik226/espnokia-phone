#include "copa_watch.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include "copamodel.h"
#include "drivers/buzzer.h"
#include "drivers/rtc.h"
#include "net/http.h"
#include "net/wifi.h"
#include "ui/goal_fx.h"

namespace copa_watch {

static const uint32_t kPollMs = 60000;  // cadencia dentro da janela do jogo
static const int32_t kJanelaMin = 150;  // kickoff ate +150 min cobre acrescimos

static Preferences prefs_;
static bool loaded_ = false;
static bool armed_ = false;
static char t1_[6], t2_[6];
static uint8_t d_, mo_, h_, mi_;
static int8_t s1_ = -1, s2_ = -1;  // baseline em RAM; -1 = ainda sem fetch
static uint32_t next_ = 0;
static CopaJogo jogos_[8];
static char buf_[3072];

static void load() {
  if (loaded_) return;
  prefs_.begin("espnokia");
  armed_ = prefs_.getBool("cw_on", false);
  prefs_.getString("cw_t1", t1_, sizeof(t1_));
  prefs_.getString("cw_t2", t2_, sizeof(t2_));
  d_ = prefs_.getUChar("cw_d", 0);
  mo_ = prefs_.getUChar("cw_mo", 0);
  h_ = prefs_.getUChar("cw_h", 0);
  mi_ = prefs_.getUChar("cw_mi", 0);
  loaded_ = true;
}

void init() { load(); }

void arm(const char* t1, const char* t2, uint8_t dia, uint8_t mes,
         uint8_t h, uint8_t m) {
  load();
  armed_ = true;
  snprintf(t1_, sizeof(t1_), "%s", t1);
  snprintf(t2_, sizeof(t2_), "%s", t2);
  d_ = dia; mo_ = mes; h_ = h; mi_ = m;
  s1_ = s2_ = -1;
  next_ = 0;
  prefs_.putBool("cw_on", true);
  prefs_.putString("cw_t1", t1_);
  prefs_.putString("cw_t2", t2_);
  prefs_.putUChar("cw_d", d_);
  prefs_.putUChar("cw_mo", mo_);
  prefs_.putUChar("cw_h", h_);
  prefs_.putUChar("cw_mi", mi_);
}

void disarm() {
  load();
  armed_ = false;
  prefs_.putBool("cw_on", false);
}

bool armed(const char* t1, const char* t2) {
  load();
  return armed_ && strcmp(t1, t1_) == 0 && strcmp(t2, t2_) == 0;
}

// mesmo "minuto do ano" aproximado do alarme: basta pra janela do jogo
static int32_t minuto(uint8_t mes, uint8_t dia, uint8_t h, uint8_t m) {
  return (((int32_t)mes * 31 + dia) * 24 + h) * 60 + m;
}

void tick(uint32_t now) {
  if (!armed_ || goal_fx::active()) return;
  if (next_ && (int32_t)(now - next_) < 0) return;
  next_ = now + kPollMs;  // erro tambem espera o ciclo: sem martelar o server
  if (!wifi::connected()) return;
  rtc::DateTime dt;
  if (!rtc::now(dt)) return;  // cacheia 1 s, barato por loop
  int32_t delta = minuto(dt.month, dt.day, dt.hour, dt.min) -
                  minuto(mo_, d_, h_, mi_);
  if (delta < 0) return;  // nem comecou (o aviso de inicio e do alarme)
  if (delta > kJanelaMin) { disarm(); return; }
  if (buzzer::busy()) { next_ = now + 200; return; }  // GET seguraria o bip
  if (http::get_json("/copa/live", buf_, sizeof(buf_)) != 200) return;
  uint8_t n = copa_parse(buf_, jogos_, 8, nullptr);
  for (uint8_t i = 0; i < n; i++) {
    const CopaJogo& j = jogos_[i];
    if (strcmp(j.t1, t1_) != 0 || strcmp(j.t2, t2_) != 0) continue;
    if (s1_ >= 0 && (j.s1 > s1_ || j.s2 > s2_)) {  // 1o fetch so faz baseline
      char placar[24];
      copa_linha(j, placar, sizeof(placar));
      const char* autor =
          (j.s1 > s1_) ? copa_ultimo_gol(j.g1) : copa_ultimo_gol(j.g2);
      goal_fx::start(placar, autor);
    }
    s1_ = j.s1; s2_ = j.s2;
    return;
  }
  if (s1_ >= 0) disarm();  // estava no feed e saiu: jogo terminou
}

}  // namespace copa_watch
