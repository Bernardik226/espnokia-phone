#pragma once
#include <stdint.h>

// Acerta o RTC via NTP quando o WiFi conecta — so se o toggle das Config
// (Data e hora > Sincronizar) estiver ligado. Re-sync a cada 24 h.
namespace ntp {
void tick(uint32_t now_ms);
}  // namespace ntp
