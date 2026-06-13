#pragma once
#include <stddef.h>
#include <stdint.h>

// Upload de voz por HTTP chunked (Transfer-Encoding: chunked) direto no
// WiFiClient. Fluxo: begin -> write (um chunk por chamada, enquanto grava)
// -> finish_begin (fecha o corpo) -> finish_poll a cada tick ate a resposta
// (kEsperando enquanto nada chega: a tela segue animando, sem bloquear).
// O socket e keep-alive: conecta() antecipa o handshake TLS (~1-2 s) pra
// fora do aperto do botao e a resposta deixa a conexao viva pra proxima fala.
namespace voicecli {
static const int kEsperando = -100;          // finish_poll: resposta a caminho
bool conecta();                              // abre TCP+TLS (ou ja vivo)
bool ping();                                 // GET /health segura o socket
bool begin(const char* path);                // manda os headers do POST
bool write(const uint8_t* dados, size_t n);  // false = conexao caiu
bool finish_begin();                         // fecha o corpo chunked
int finish_poll(char* resp, size_t resp_len);  // status, <0 rede, kEsperando
void abort();                                // desiste e solta o socket
}  // namespace voicecli
