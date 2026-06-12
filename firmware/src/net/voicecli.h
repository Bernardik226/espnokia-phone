#pragma once
#include <stddef.h>
#include <stdint.h>

// Upload de voz por HTTP chunked (Transfer-Encoding: chunked) direto no
// WiFiClient. Fluxo: begin -> write (um chunk por chamada, enquanto grava)
// -> finish (fecha o corpo, espera a resposta ate 20 s e copia o JSON).
namespace voicecli {
bool begin(const char* path);                // conecta e manda os headers
bool write(const uint8_t* dados, size_t n);  // false = conexao caiu
int finish(char* resp, size_t resp_len);     // status HTTP ou <0 (rede)
void abort();                                // desiste e solta o socket
}  // namespace voicecli
