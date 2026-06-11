#include "rtttl.h"
#include <string.h>
#include <ctype.h>

// semitom 0=c ... 11=b, oitava 4; oitavas maiores = << (o-4)
static const uint16_t kOct4[12] = {262,277,294,311,330,349,370,392,415,440,466,494};
static const int8_t kIdx[7] = {9,11,0,2,4,5,7};  // a b c d e f g

static uint16_t read_num(const char*& p) {
  uint16_t v = 0;
  while (isdigit((unsigned char)*p)) v = v * 10 + (*p++ - '0');
  return v;
}

bool Rtttl::begin(const char* tune) {
  const char* p = strchr(tune, ':');
  if (!p) return false;
  p++;  // seção de defaults
  while (*p && *p != ':') {
    if (*p == 'd') { p += 2; def_dur_ = (uint8_t)read_num(p); }
    else if (*p == 'o') { p += 2; def_oct_ = (uint8_t)read_num(p); }
    else if (*p == 'b') { p += 2; bpm_ = read_num(p); }
    else p++;
  }
  if (*p != ':') return false;
  if (def_dur_ == 0 || bpm_ == 0) return false;  // header malformado (d=/b= sem dígito)
  p_ = p + 1;
  return true;
}

bool Rtttl::next(RtttlNote& out) {
  if (!p_ || !*p_) return false;
  while (*p_ == ',' || *p_ == ' ') p_++;
  if (!*p_) return false;

  uint16_t dur = read_num(p_);
  if (dur == 0) dur = def_dur_;

  char c = (char)tolower((unsigned char)*p_);
  int8_t semi = -1;
  if (c >= 'a' && c <= 'g') { semi = kIdx[c - 'a']; p_++; }
  else if (c == 'p') { p_++; }
  else return false;
  if (*p_ == '#') { if (semi >= 0 && semi < 11) semi++; p_++; }  // b#/p# não estouram a tabela

  bool dotted = false;
  if (*p_ == '.') { dotted = true; p_++; }       // ponto antes ou depois da oitava
  uint16_t oct = read_num(p_);
  if (oct == 0) oct = def_oct_;
  if (*p_ == '.') { dotted = true; p_++; }

  uint32_t ms = (60000UL * 4 / bpm_) / dur;
  if (dotted) ms += ms / 2;

  if (semi >= 0 && (oct < 4 || oct > 7)) return false;  // oitava fora de 4-7: shift inválido
  out.freq_hz = (semi < 0) ? 0 : (uint16_t)(kOct4[semi] << (oct - 4));
  out.dur_ms = (uint16_t)ms;
  return true;
}
