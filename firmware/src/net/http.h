#pragma once
#include <stddef.h>

// GET no server companion (SERVER_URL do espnokia_config.h), ja com a chave
// do device no header. Retorna o codigo HTTP (200 = corpo valido em buf)
// ou negativo em erro de rede.
namespace http {
int get_json(const char* path, char* buf, size_t len);
}  // namespace http
