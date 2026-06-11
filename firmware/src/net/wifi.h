#pragma once
#include <stdint.h>

// WiFi nao-bloqueante: begin no init, reconexao com backoff no tick.
namespace wifi {
void init();
void tick(uint32_t now_ms);
bool connected();
uint8_t level();  // barras 0..4 a partir do RSSI (0 = desconectado)
}  // namespace wifi
