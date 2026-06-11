#pragma once
#include <stdint.h>

// DS3231 no I2C: hora local que persiste com a bateria + termometro embutido.
// now() le o chip no maximo 1x/s (cache interno); apps podem chamar por frame.
namespace rtc {
struct DateTime {
  uint16_t year;  // 2000..2099
  uint8_t month;  // 1..12
  uint8_t day;    // 1..31
  uint8_t dow;    // 0..6, 0=domingo
  uint8_t hour;   // 0..23
  uint8_t min;
  uint8_t sec;
};
// true se o DS3231 respondeu no barramento; se a bateria zerou (flag OSF),
// semeia com a hora de compilacao ate o acerto manual/NTP
bool init();
bool present();
bool now(DateTime& dt);          // hora corrente (cache de 1s); false se ausente
bool write(const DateTime& dt);  // acerta o chip e limpa o OSF
float temperature();             // graus C, resolucao 0.25
}  // namespace rtc
