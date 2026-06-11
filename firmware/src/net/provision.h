#pragma once
#include <stdint.h>

// Modo configuracao: AP local (1 conexao so) + DNS captive + pagina web
// pra cadastrar a rede WiFi. O POST /salvar grava cifrado (credstore) e
// reinicia; o boot seguinte sobe em STA com a rede nova.
namespace provision {
void start();
void tick(uint32_t now_ms);
const char* ap_name();   // SSID do AP (espnokia-XXXX)
}  // namespace provision
