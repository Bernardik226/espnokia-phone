#pragma once
#include <stdint.h>

// Utilidades de tempo puras (sem depender do Arduino), compartilhadas entre
// alarm.cpp, copa_watch.cpp, jogo_aviso.cpp, buzzer.cpp e provision.cpp.
namespace timeutil {

// "minuto do ano" aproximado: basta pra comparar datas dentro da mesma
// janela de poucos minutos/dias (nao lida com virada de ano).
inline int32_t minuto(uint8_t mes, uint8_t dia, uint8_t h, uint8_t m) {
  return (((int32_t)mes * 31 + dia) * 24 + h) * 60 + m;
}

// comparação imune ao rollover de millis()
inline bool reached(uint32_t now, uint32_t target) {
  return (int32_t)(now - target) >= 0;
}

}  // namespace timeutil
