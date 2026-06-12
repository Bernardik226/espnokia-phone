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
    copia(g.min, sizeof(g.min), j["min"]);
    copia(g.est, sizeof(g.est), j["est"]);
    copia(g.g1, sizeof(g.g1), j["g1"]);
    copia(g.g2, sizeof(g.g2), j["g2"]);
    copia(g.n1, sizeof(g.n1), j["n1"]);
    copia(g.n2, sizeof(g.n2), j["n2"]);
    n++;
  }
  return n;
}

uint8_t copa_parse_grupos(const char* json, CopaGrupo* gs, uint8_t max) {
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return 0;
  JsonArray arr = doc["grupos"];
  if (arr.isNull()) return 0;

  uint8_t n = 0;
  for (JsonObject g : arr) {
    if (n >= max) break;
    CopaGrupo& grp = gs[n];
    copia(grp.nome, sizeof(grp.nome), g["n"]);
    grp.nt = 0;
    for (JsonObject t : (JsonArray)g["t"]) {
      if (grp.nt >= 4) break;
      CopaGrupoTime& gt = grp.t[grp.nt];
      copia(gt.c, sizeof(gt.c), t["c"]);
      gt.pts = t["p"] | 0;
      gt.j = t["j"] | 0;
      gt.sg = t["s"] | 0;
      grp.nt++;
    }
    n++;
  }
  return n;
}

uint8_t fut_parse_ligas(const char* json, FutLiga* ligas, uint8_t max) {
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return 0;
  JsonArray arr = doc["ligas"];
  if (arr.isNull()) return 0;

  uint8_t n = 0;
  for (JsonObject l : arr) {
    if (n >= max) break;
    copia(ligas[n].id, sizeof(ligas[n].id), l["id"]);
    copia(ligas[n].n, sizeof(ligas[n].n), l["n"]);
    if (!ligas[n].id[0]) continue;  // sem id nao tem como buscar os jogos
    n++;
  }
  return n;
}

const char* copa_ultimo_gol(const char* gols) {
  const char* p = strrchr(gols, '\n');
  return p ? p + 1 : gols;
}

void copa_linha(const CopaJogo& j, char* out, size_t len) {
  if (j.s1 >= 0 && j.s2 >= 0)
    snprintf(out, len, "%s %dx%d %s", j.t1, j.s1, j.s2, j.t2);
  else
    snprintf(out, len, "%u/%u %s x %s", j.dia, j.mes, j.t1, j.t2);
}
