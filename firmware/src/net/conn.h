#pragma once
#include <stddef.h>

// Identidade de conexao do device, persistida na NVS: a chave (sorteada na
// 1a vez, ou semeada do espnokia_config.h) e o endereco do server (editavel
// pela pagina AP). O config.h vira so o padrao de fabrica — quem usa configura
// pelo aparelho, sem recompilar.
namespace conn {
void init();                            // carrega a NVS; sorteia a chave se preciso
const char* server_url();               // "https://host" (sem barra no fim)
const char* device_key();               // 32 hex; a credencial do device
void set_server_url(const char* url);   // grava o endereco na NVS e passa a valer
// monta "server_url/#k=chave" pro QR de pareamento; devolve out
const char* pair_link(char* out, size_t len);

// nome do header HTTP que carrega a chave do device: usado por http.cpp
// (GET simples) e voicecli.cpp (upload de voz), pra nao divergir se um dia
// precisar renomear
constexpr char kHeaderDeviceKey[] = "X-Device-Key";
}  // namespace conn
