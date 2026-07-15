#pragma once
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

// Escolhe entre o client TLS (WiFiClientSecure) e o client plano
// (WiFiClient) conforme https, ativando modo inseguro na TLS quando
// necessario. Server em host com TLS tambem funciona: sem validar a CA
// (o ESP32 nao tem bundle atualizado) — o segredo do device e a chave,
// nao o canal.
// Nao possui os clients (plain/tls): quem chama continua dono do lifetime
// de ambos, exatamente como antes desta funcao existir — a funcao so
// decide qual dos dois objetos ja existentes usar.
namespace net {
inline WiFiClient* abrir_cliente(bool https, WiFiClient& plain,
                                  WiFiClientSecure& tls) {
  if (https) tls.setInsecure();
  return https ? (WiFiClient*)&tls : &plain;
}
}  // namespace net
