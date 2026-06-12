#pragma once
#include <stddef.h>
#include <stdint.h>

// Upload de voz por HTTP chunked (Transfer-Encoding: chunked) direto no
// WiFiClient. Fluxo: begin -> write (um chunk por chamada, enquanto grava)
// -> finish (fecha o corpo, espera a resposta ate 20 s e copia o JSON).
// O socket e keep-alive: conecta() antecipa o handshake TLS (~1-2 s) pra
// fora do aperto do botao e finish() deixa a conexao viva pra proxima fala.
namespace voicecli {
bool conecta();                              // abre TCP+TLS (ou ja vivo)
bool ping();                                 // GET /health segura o socket
bool begin(const char* path);                // manda os headers do POST
bool write(const uint8_t* dados, size_t n);  // false = conexao caiu
int finish(char* resp, size_t resp_len);     // status HTTP ou <0 (rede)
void abort();                                // desiste e solta o socket
}  // namespace voicecli
