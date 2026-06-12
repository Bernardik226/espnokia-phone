#include "claudemodel.h"

namespace claude {

// days-from-civil (Howard Hinnant): calendário gregoriano em O(1)
int32_t dias_civis(uint16_t y, uint8_t m, uint8_t d) {
  int32_t yy = (int32_t)y - (m <= 2);
  int32_t era = (yy >= 0 ? yy : yy - 399) / 400;
  uint32_t yoe = (uint32_t)(yy - era * 400);
  uint32_t doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

int64_t epoch_min(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mi) {
  return (int64_t)dias_civis(y, m, d) * 24 * 60 + (int64_t)h * 60 + mi;
}

Humor humor(int64_t agora_min, int64_t ultima_min, uint8_t hora_local) {
  if (hora_local >= 23 || hora_local < 7) return H_DORMINDO;
  if (ultima_min <= 0) return H_NEUTRO;
  int64_t delta = agora_min - ultima_min;
  if (delta < 0) return H_NEUTRO;  // relogio regrediu: trata como nunca conversou
  if (delta < 24 * 60) return H_FELIZ;
  if (delta <= 3 * 24 * 60) return H_NEUTRO;
  return H_CARENTE;
}

}  // namespace claude
