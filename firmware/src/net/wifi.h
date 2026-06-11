#pragma once
#include <stddef.h>
#include <stdint.h>

// WiFi nao-bloqueante. Com rede salva (credstore) sobe em STA e reconecta
// com backoff no tick; sem rede (ou apos "trocar rede") sobe o modo config
// (provision: AP + pagina web). forget/reconfigure reiniciam o aparelho.
namespace wifi {
void init();
void tick(uint32_t now_ms);
bool connected();
bool provisioning();              // modo config (AP) ativo?
uint8_t level();                  // barras 0..4 a partir do RSSI (0 = desconectado)
const char* ssid();               // rede salva, ou nome do AP no modo config
const char* ap_pass();            // senha do AP no modo config ("" fora dele)
void ip_str(char* buf, size_t len);
void forget();                    // apaga a rede e reinicia no modo config
void reconfigure();               // mantem a rede e reinicia no modo config
}  // namespace wifi
