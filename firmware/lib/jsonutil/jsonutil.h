#pragma once
#include <stddef.h>
#include <stdio.h>
#include <ArduinoJson.h>

// Utilidades compartilhadas de parsing JSON (ArduinoJson) usadas por
// claudemodel e copamodel.
namespace jsonutil {

// desserializa json em doc; true no sucesso (mesmo formato da guarda
// "if (deserializeJson(...)) return ...;" repetida em cada parser)
inline bool parse_json(const char* json, JsonDocument& doc) {
  return deserializeJson(doc, json) == DeserializationError::Ok;
}

// copia campo com truncamento null-safe: src nulo (campo ausente/tipo
// errado no JSON) vira ""
inline void copia(char* dst, size_t cap, const char* src) {
  if (!src) src = "";
  snprintf(dst, cap, "%s", src);  // trunca com nul garantido
}

}  // namespace jsonutil
