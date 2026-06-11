#include "copamodel.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

static void copia(char* dst, size_t cap, const char* src) {
  if (!src) src = "";
  snprintf(dst, cap, "%s", src);  // trunca com nul garantido
}

uint8_t copa_parse(const char* json, CopaJogo* jogos, uint8_t max,
                   uint32_t* atualizado_s) {
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return 0;
  JsonArray arr = doc["jogos"];
  if (arr.isNull()) return 0;
  if (atualizado_s) *atualizado_s = doc["atualizado_s"] | 0;

  uint8_t n = 0;
  for (JsonObject j : arr) {
    if (n >= max) break;
    CopaJogo& g = jogos[n];
    g.dia = j["dia"] | 0;
    g.mes = j["mes"] | 0;
    g.h = j["h"] | 0;
    g.m = j["m"] | 0;
    copia(g.t1, sizeof(g.t1), j["t1"]);
    copia(g.t2, sizeof(g.t2), j["t2"]);
    copia(g.info, sizeof(g.info), j["info"]);
    g.s1 = j["s1"] | -1;
    g.s2 = j["s2"] | -1;
    g.live = j["live"] | false;
    n++;
  }
  return n;
}

void copa_linha(const CopaJogo& j, char* out, size_t len) {
  if (j.s1 >= 0 && j.s2 >= 0)
    snprintf(out, len, "%s %dx%d %s", j.t1, j.s1, j.s2, j.t2);
  else
    snprintf(out, len, "%u/%u %s x %s", j.dia, j.mes, j.t1, j.t2);
}
