#pragma once
#include <stddef.h>

// Credenciais WiFi cifradas na NVS. A chave vem do eFuse MAC do proprio
// chip: um dump da flash de outro device (ou so da particao NVS) nao decifra
// a senha, porque o eFuse nao sai no dump. Nao protege de quem tem o device
// E o firmware na mao — protege do vazamento casual de flash copiada.
namespace credstore {
bool present();                                      // tem rede salva?
bool load(char* ssid, size_t ssid_len, char* pass, size_t pass_len);
void save(const char* ssid, const char* pass);       // sobrescreve a salva
void forget();                                       // apaga a rede
bool take_force_ap();   // flag one-shot "abrir modo config no proximo boot"
void set_force_ap();
}  // namespace credstore
