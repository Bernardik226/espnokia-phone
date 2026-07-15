#include "claudemodel.h"
#include <stdio.h>
#include <string.h>
#include <ArduinoJson.h>
#include "jsonutil.h"

namespace claude {

// days-from-civil (Howard Hinnant): calendário gregoriano em O(1)
int32_t dias_civis(uint16_t y, uint8_t m, uint8_t d) {
  int32_t yy = (int32_t)y - (m <= 2);
  int32_t era = (yy >= 0 ? yy : yy - 399) / 400;
  uint32_t yoe = (uint32_t)(yy - era * 400);
  uint32_t doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

int64_t epoch_min(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mi) {
  return (int64_t)dias_civis(y, m, d) * 24 * 60 + (int64_t)h * 60 + mi;
}

Humor humor(int64_t agora_min, int64_t ultima_min, uint8_t hora_local) {
  if (hora_local >= 23 || hora_local < 7) return H_DORMINDO;
  if (ultima_min <= 0) return H_NEUTRO;
  int64_t delta = agora_min - ultima_min;
  if (delta < 0) return H_NEUTRO;  // relogio regrediu: trata como nunca conversou
  if (delta < 24 * 60) return H_FELIZ;
  if (delta <= 3 * 24 * 60) return H_NEUTRO;
  return H_CARENTE;
}

static void muda(Maquina& m, Estado st, uint32_t now_ms) {
  m.st = st;
  m.desde_ms = now_ms;
}

void maquina_init(Maquina& m, uint32_t now_ms) { muda(m, E_PET, now_ms); }

bool maquina_evento(Maquina& m, Evento ev, uint32_t now_ms) {
  switch (m.st) {
    case E_PET:
      if (ev == EV_OK_APERTA) { muda(m, E_OUVINDO, now_ms); return true; }
      break;
    case E_OUVINDO:
      if (ev == EV_OK_SOLTA) { muda(m, E_PENSANDO, now_ms); return true; }
      break;
    case E_PENSANDO:
      if (ev == EV_RESPOSTA || ev == EV_FALHA) {
        muda(m, E_FALANDO, now_ms);
        return true;
      }
      break;
    case E_FALANDO:
      if (ev == EV_VOLTA) { muda(m, E_PET, now_ms); return true; }
      break;
  }
  return false;
}

bool maquina_tick(Maquina& m, uint32_t now_ms) {
  uint32_t dt = now_ms - m.desde_ms;
  if (m.st == E_OUVINDO && dt >= kOuvindoMaxMs) {
    muda(m, E_PENSANDO, now_ms);
    return true;
  }
  if (m.st == E_FALANDO && dt >= kFalandoMaxMs) {
    muda(m, E_PET, now_ms);
    return true;
  }
  return false;
}

// avanca uma linha a partir de t[ini]; poe em *fim o byte logo apos o
// ultimo char visivel e devolve onde a proxima linha comeca
static size_t avanca_linha(const char* t, size_t ini, uint8_t cols,
                           size_t* fim) {
  size_t i = ini, ultimo_espaco = ini, colunas = 0;
  while (t[i] && colunas < cols) {
    if (t[i] == '\n') { *fim = i; return i + 1; }
    if (t[i] == ' ') ultimo_espaco = i;
    i++;
    while ((t[i] & 0xC0) == 0x80) i++;  // pula continuacao UTF-8
    colunas++;
  }
  if (!t[i]) { *fim = i; return i; }                 // acabou o texto
  if (t[i] == '\n') { *fim = i; return i + 1; }
  if (t[i] == ' ') { *fim = i; return i + 1; }       // quebra exata
  if (ultimo_espaco > ini) { *fim = ultimo_espaco; return ultimo_espaco + 1; }
  *fim = i;                                          // palavrao: corta seco
  return i;
}

uint16_t linhas_total(const char* texto, uint8_t cols) {
  if (!texto || !texto[0] || cols == 0) return 0;
  uint16_t n = 0;
  size_t i = 0, fim;
  while (texto[i]) { i = avanca_linha(texto, i, cols, &fim); n++; }
  return n;
}

bool linha_texto(const char* texto, uint16_t n, uint8_t cols,
                 char* out, size_t out_len) {
  if (!texto || !texto[0] || cols == 0 || out_len == 0) return false;
  size_t i = 0, fim = 0;
  for (uint16_t k = 0; texto[i]; k++) {
    size_t prox = avanca_linha(texto, i, cols, &fim);
    if (k == n) {
      size_t len = fim - i;
      if (len >= out_len) len = out_len - 1;
      memcpy(out, texto + i, len);
      out[len] = '\0';
      return true;
    }
    i = prox;
  }
  return false;
}

// reduz o caractere em s[pos] pra ASCII minusculo; acentos pt-BR (2 bytes,
// 0xC3 xx) viram a letra base, o resto de multibyte vira espaco
static char base_ascii(const char* s, size_t pos, size_t* adv) {
  unsigned char c = (unsigned char)s[pos];
  if (c < 0x80) {
    *adv = 1;
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
  }
  size_t n = 1;
  while ((s[pos + n] & 0xC0) == 0x80) n++;
  *adv = n;
  if (c == 0xC3) {
    unsigned char x = (unsigned char)s[pos + 1] | 0x20;  // minusculiza
    if (x >= 0xA0 && x <= 0xA5) return 'a';
    if (x >= 0xA8 && x <= 0xAB) return 'e';
    if (x >= 0xAC && x <= 0xAF) return 'i';
    if ((x >= 0xB2 && x <= 0xB6) || x == 0xB8) return 'o';
    if (x >= 0xB9 && x <= 0xBC) return 'u';
    if (x == 0xA7) return 'c';                           // ç
    if (x == 0xB1) return 'n';                           // ñ
  }
  return ' ';
}

bool bitspeech_next(const char* texto, size_t pos, Tom& t, size_t* prox) {
  if (!texto || !texto[pos]) return false;
  size_t adv;
  char c = base_ascii(texto, pos, &adv);
  *prox = pos + adv;
  if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
    // sol5 la5 si5 fa#5 mi5 — cada vogal canta sempre na mesma nota
    static const uint16_t kVogal[] = {784, 880, 988, 740, 659};
    const char* v = "aeiou";
    t.freq = kVogal[strchr(v, c) - v];
    t.dur_ms = 90;
  } else if (c >= 'b' && c <= 'z') {
    t.freq = (uint16_t)(330 + (c - 'a') * 10);  // grave, varia com a letra
    t.dur_ms = 45;
  } else {
    t.freq = 0;                                 // espaco/pontuacao: respiro
    t.dur_ms = 70;
  }
  return true;
}

bool voz_parse(const char* json, char* falei, size_t falei_len,
               char* resposta, size_t resp_len) {
  JsonDocument doc;
  if (!jsonutil::parse_json(json, doc)) return false;
  const char* r = doc["resposta"];
  if (!r || !r[0]) return false;
  snprintf(resposta, resp_len, "%s", r);
  if (falei && falei_len) {
    jsonutil::copia(falei, falei_len, doc["falei"]);
  }
  return true;
}

uint8_t registro_parse(const char* json, RegPar* itens, uint8_t max,
                       uint16_t* total, uint8_t* pags, uint8_t* pag) {
  *total = 0; *pags = 0; *pag = 0;
  JsonDocument doc;
  if (!jsonutil::parse_json(json, doc)) return 0;
  *total = doc["total"] | 0;
  *pags = doc["pags"] | 0;
  *pag = doc["pag"] | 0;
  uint8_t n = 0;
  for (JsonObject it : doc["itens"].as<JsonArray>()) {
    if (n >= max) break;
    jsonutil::copia(itens[n].q, sizeof(itens[n].q), it["q"]);
    jsonutil::copia(itens[n].r, sizeof(itens[n].r), it["r"]);
    jsonutil::copia(itens[n].d, sizeof(itens[n].d), it["d"]);
    jsonutil::copia(itens[n].h, sizeof(itens[n].h), it["h"]);
    n++;
  }
  return n;
}

static const uint8_t kRegCols = 16;

// monta "<prefixo>: <txt>" e conta/copia a linha pedida
static uint16_t reg_bloco(const char* prefixo, const char* txt,
                          uint16_t pula, char* out, size_t out_len,
                          bool* achou) {
  char tmp[300];  // prefixo curto + r[284]
  snprintf(tmp, sizeof(tmp), "%s: %s", prefixo, txt);
  uint16_t l = linhas_total(tmp, kRegCols);
  if (out && pula < l) {
    *achou = linha_texto(tmp, pula, kRegCols, out, out_len);
  }
  return l;
}

bool registro_linha(const RegPar* itens, uint8_t n_itens,
                    const char* rotulo_eu, uint16_t n,
                    char* out, size_t out_len, uint8_t* par_idx) {
  uint16_t base = 0;
  for (uint8_t i = 0; i < n_itens; i++) {
    bool achou = false;
    uint16_t lq = reg_bloco(rotulo_eu, itens[i].q, n - base,
                            n >= base ? out : nullptr, out_len, &achou);
    if (achou) { if (par_idx) *par_idx = i; return true; }
    base += lq;
    uint16_t lr = reg_bloco("CLAWD", itens[i].r, n - base,
                            n >= base ? out : nullptr, out_len, &achou);
    if (achou) { if (par_idx) *par_idx = i; return true; }
    base += lr;
  }
  return false;
}

uint16_t registro_linhas_total(const RegPar* itens, uint8_t n_itens,
                               const char* rotulo_eu) {
  uint16_t base = 0;
  bool dummy;
  for (uint8_t i = 0; i < n_itens; i++) {
    base += reg_bloco(rotulo_eu, itens[i].q, 0, nullptr, 0, &dummy);
    base += reg_bloco("CLAWD", itens[i].r, 0, nullptr, 0, &dummy);
  }
  return base;
}

}  // namespace claude
