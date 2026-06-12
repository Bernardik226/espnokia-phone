#include "jogo_aviso.h"
#include <stdio.h>
#include "alarm.h"
#include "copa_watch.h"
#include "i18n.h"
#include "drivers/rtc.h"

// mesmo "minuto do ano" aproximado do alarme: so compara antes/depois
static int32_t minuto_(uint8_t mes, uint8_t dia, uint8_t h, uint8_t m) {
  return (((int32_t)mes * 31 + dia) * 24 + h) * 60 + m;
}

bool jogo_aviso_armado(const CopaJogo& j) {
  return alarme::armed(j.dia, j.mes, j.h, j.m) ||
         copa_watch::armed(j.t1, j.t2);
}

bool jogo_aviso_toggle(const CopaJogo& j, const char* live_path) {
  if (jogo_aviso_armado(j)) {
    if (alarme::armed(j.dia, j.mes, j.h, j.m)) alarme::clear();
    copa_watch::disarm();
    return false;
  }
  char lbl[16];
  snprintf(lbl, sizeof(lbl), "%s x %s", j.t1, j.t2);
  rtc::DateTime dt;
  bool futuro = rtc::now(dt) &&
                minuto_(dt.month, dt.day, dt.hour, dt.min) <
                    minuto_(j.mes, j.dia, j.h, j.m);
  if (futuro) alarme::set(STR_GAME_NOW, j.dia, j.mes, j.h, j.m, lbl);
  copa_watch::arm(j.t1, j.t2, j.dia, j.mes, j.h, j.m, live_path);
  return true;
}
