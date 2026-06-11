#include "net/ntp.h"
#include <Arduino.h>
#include <time.h>
#include "apps/app_settings.h"
#include "drivers/rtc.h"
#include "net/wifi.h"

namespace ntp {

static uint32_t next_sync_ = 0;   // proximo sync agendado (ms); 0 = nunca sincronizou
static uint32_t last_try_ = 0;
static bool configurado_ = false;

void tick(uint32_t now_ms) {
  if (!settings_ntp_sync_enabled() || !wifi::connected()) return;
  if (next_sync_ && (int32_t)(now_ms - next_sync_) < 0) return;
  if (now_ms - last_try_ < 1000) return;  // checa no maximo 1x/s
  last_try_ = now_ms;

  if (!configurado_) {
    configTime(-3 * 3600, 0, "pool.ntp.org");  // Brasilia, sem horario de verao
    configurado_ = true;
  }
  struct tm t;
  if (!getLocalTime(&t, 0)) return;  // SNTP ainda nao respondeu; tenta depois

  rtc::DateTime dt = {(uint16_t)(t.tm_year + 1900), (uint8_t)(t.tm_mon + 1),
                      (uint8_t)t.tm_mday,           (uint8_t)t.tm_wday,
                      (uint8_t)t.tm_hour,           (uint8_t)t.tm_min,
                      (uint8_t)t.tm_sec};
  if (rtc::write(dt))
    Serial.printf("[ntp] rtc acertado: %04u-%02u-%02u %02u:%02u:%02u\n",
                  dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec);
  next_sync_ = now_ms + 24UL * 3600 * 1000;  // re-sync em 24 h
}

}  // namespace ntp
