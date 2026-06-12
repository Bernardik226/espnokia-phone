#include "app_claude.h"

#include <Preferences.h>
#include <U8g2lib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "claudemodel.h"
#include "clockfmt.h"
#include "i18n.h"
#include "drivers/buzzer.h"
#include "drivers/mic.h"
#include "drivers/rtc.h"
#include "net/http.h"
#include "net/voicecli.h"
#include "net/wifi.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Pet do Claude: segura OK pra falar (INMP441 -> POST chunked), solta pra
// pensar; a resposta sai em typewriter com bitspeech no buzzer. Humor pelo
// carinho (ultima conversa na NVS) + janela de sono via RTC.

static claude::Maquina maq_;
static int64_t ultima_min_ = 0;   // ultima conversa (minutos civis, NVS)
static char texto_[512];          // resposta (ou erro local) na tela
static bool gravando_ = false;
static uint32_t grava_t0_ = 0;    // contador de segundos do OUVINDO
static uint16_t nivel_ = 0;       // media |amostra| da ultima leitura (VU)
static uint32_t finish_em_ = 0;   // agenda o finish() 1 frame depois
static uint32_t ping_em_ = 0;     // proximo ping de keep-alive do socket
static size_t fala_pos_ = 0;      // typewriter/bitspeech: posicao no texto
static uint32_t pausa_ate_ = 0;   // respiro do bitspeech (freq 0)
static bool fala_viva_ = false;   // typewriter rodando (so na 1a passada)
static uint8_t pag_ = 0;

// ---- registro de conversa (lista de registros -> leitura do par) ----
enum RegFetch : uint8_t { REG_PENDING, REG_OK, REG_ERR, REG_NONET };
static bool registro_ = false;
static bool reg_lendo_ = false;       // false = lista, true = leitura
static RegFetch reg_fetch_ = REG_OK;
static uint32_t reg_fetch_ms_ = 0;
static char reg_buf_[4096];           // pagina de 6 pares (~2,6 KB no pior)
static claude::RegPar reg_itens_[6];
static uint8_t reg_n_ = 0;
static uint16_t reg_total_ = 0;
static uint8_t reg_pags_ = 0, reg_pag_ = 0, reg_alvo_ = 0;
static uint8_t reg_sel_ = 0;          // registro selecionado na lista
static bool reg_sel_fim_ = false;     // subiu de pagina: seleciona o ultimo
static uint16_t reg_linha_ = 0, reg_nlinhas_ = 0;  // rolagem da leitura

static void reg_pede_pagina(uint8_t pag, bool sel_fim) {
  reg_alvo_ = pag;
  reg_sel_fim_ = sel_fim;
  reg_fetch_ = wifi::connected() ? REG_PENDING : REG_NONET;
  reg_fetch_ms_ = millis();
}

static const uint8_t kCols = 16;  // fonte 3310 small: 16 col x 5 linhas
static const uint8_t kRows = 5;

static uint8_t hora_agora() {
  rtc::DateTime dt;
  return rtc::now(dt) ? dt.hour : 12;  // sem RTC: sempre acordado
}

static int64_t agora_min() {
  rtc::DateTime dt;
  if (!rtc::now(dt)) return 0;
  return claude::epoch_min(dt.year, dt.month, dt.day, dt.hour, dt.min);
}

static uint8_t paginas() {
  uint16_t l = claude::linhas_total(texto_, kCols);
  return l ? (uint8_t)((l - 1) / kRows + 1) : 1;
}

static void comeca_fala(uint32_t now) {
  fala_pos_ = 0;
  pausa_ate_ = 0;
  pag_ = 0;
  fala_viva_ = true;
  maq_.desde_ms = now;  // renova o timeout de 60 s do FALANDO
}

// erro local (sem wifi / sem conexao): pula direto pro FALANDO com aviso
static void mostra_aviso(StrId id, uint32_t now) {
  snprintf(texto_, sizeof(texto_), "%s", tr(id));
  maq_.st = claude::E_FALANDO;
  comeca_fala(now);
}

static void encerra_gravacao(uint32_t now) {
  mic::stop();
  gravando_ = false;
  finish_em_ = now + 80;  // 1 frame de PENSANDO na tela antes do bloqueio
  if (!finish_em_) finish_em_ = 1;  // 0 e sentinela de "nada agendado"
}

static void on_enter() {
  claude::maquina_init(maq_, millis());
  Preferences p;
  p.begin("claude", true);
  ultima_min_ = p.getLong64("ult", 0);
  p.end();
  texto_[0] = '\0';
  gravando_ = false;
  finish_em_ = 0;
  fala_viva_ = false;
  registro_ = false;
  mic::init();
  // handshake TLS antecipado: apertar OK ja encontra o socket quente
  if (wifi::connected()) voicecli::conecta();
}

static void on_exit() {
  if (gravando_) {
    mic::stop();
    gravando_ = false;
  }
  voicecli::abort();
  finish_em_ = 0;
  registro_ = false;
}

static bool input(Button b, BtnEvent e) {
  uint32_t now = millis();
  if (registro_) {  // a tela de registro engole tudo menos o C
    if (e != EV_PRESS) return true;
    if (reg_fetch_ != REG_OK) {  // buscando/erro: so C sai
      if (b == BTN_C) registro_ = false;
      return true;
    }
    if (reg_lendo_) {  // leitura do par: rola linha a linha, C volta a lista
      if (b == BTN_C) { reg_lendo_ = false; return true; }
      if (b == BTN_DOWN && reg_linha_ + 4 < reg_nlinhas_) reg_linha_++;
      if (b == BTN_UP && reg_linha_ > 0) reg_linha_--;
      return true;
    }
    if (b == BTN_C) { registro_ = false; return true; }
    if (b == BTN_OK && reg_n_) {  // abre a leitura do registro selecionado
      reg_lendo_ = true;
      reg_linha_ = 0;
      reg_nlinhas_ = claude::registro_linhas_total(&reg_itens_[reg_sel_], 1,
                                                   tr(STR_ME));
      return true;
    }
    if (b == BTN_DOWN) {
      if (reg_sel_ + 1 < reg_n_) reg_sel_++;
      else if (reg_pag_ + 1 < reg_pags_) reg_pede_pagina(reg_pag_ + 1, false);
      return true;
    }
    if (b == BTN_UP) {
      if (reg_sel_ > 0) reg_sel_--;
      else if (reg_pag_ > 0) reg_pede_pagina(reg_pag_ - 1, true);
      return true;
    }
    return true;
  }
  if (maq_.st == claude::E_PET) {
    if (e == EV_PRESS && b == BTN_OK) {
      if (!wifi::connected()) {
        mostra_aviso(STR_CONNECT_WIFI, now);
        return true;
      }
      static const char* kLang[LANG_COUNT] = {"pt", "en", "es",
                                              "fr", "de", "fi"};
      char path[28];
      snprintf(path, sizeof(path), "/claude/voz?lang=%s", kLang[i18n_lang()]);
      if (!voicecli::begin(path)) {
        mostra_aviso(STR_NO_RESPONSE, now);
        return true;
      }
      buzzer::stop();  // mata o bip da tecla OK: vazaria pro inicio da gravacao
      mic::start();
      gravando_ = true;
      grava_t0_ = millis();  // o begin bloqueia: conta so a gravacao real
      Serial.printf("[voz] gravando (begin levou %u ms)\n",
                    (unsigned)(grava_t0_ - now));
      nivel_ = 0;
      claude::maquina_evento(maq_, claude::EV_OK_APERTA, now);
      return true;
    }
    if (e == EV_PRESS && b == BTN_DOWN) {  // pra baixo: registro de conversa
      registro_ = true;
      reg_lendo_ = false;
      reg_pede_pagina(0, false);
      return true;
    }
    return false;  // C nao consumido: o shell fecha o app
  }
  if (maq_.st == claude::E_OUVINDO) {
    if (e == EV_RELEASE && b == BTN_OK) {
      if (now - grava_t0_ < 500) {  // clique seco: cancela e ensina a segurar
        Serial.printf("[voz] clique seco (%u ms): cancelado\n",
                      (unsigned)(now - grava_t0_));
        mic::stop();
        gravando_ = false;
        voicecli::abort();
        mostra_aviso(STR_TALK, now);
        return true;
      }
      Serial.printf("[voz] solto apos %u ms\n", (unsigned)(now - grava_t0_));
      claude::maquina_evento(maq_, claude::EV_OK_SOLTA, now);
      encerra_gravacao(now);
    }
    return true;
  }
  if (maq_.st == claude::E_FALANDO && e == EV_PRESS) {
    if (b == BTN_C) {
      claude::maquina_evento(maq_, claude::EV_VOLTA, now);
      return true;
    }
    // paginar desliga o typewriter (so roda na 1a passada) e renova o 60 s
    if (b == BTN_DOWN && pag_ + 1 < paginas()) {
      pag_++;
      fala_viva_ = false;
      maq_.desde_ms = now;
      return true;
    }
    if (b == BTN_UP && pag_ > 0) {
      pag_--;
      fala_viva_ = false;
      maq_.desde_ms = now;
      return true;
    }
    return true;
  }
  return true;  // PENSANDO engole teclas: a resposta ja esta a caminho
}

static void tick(uint32_t now) {
  // registro: GET bloqueante so depois de 1 frame de "Buscando..." na tela
  // e com o buzzer quieto (o PWM do bip ficaria preso ligado no bloqueio)
  if (registro_ && reg_fetch_ == REG_PENDING && now - reg_fetch_ms_ >= 60 &&
      !buzzer::busy()) {
    char path[32];
    snprintf(path, sizeof(path), "/claude/registro?pag=%u",
             (unsigned)reg_alvo_);
    int code = http::get_json(path, reg_buf_, sizeof(reg_buf_));
    if (code == 200) {
      reg_n_ = claude::registro_parse(reg_buf_, reg_itens_, 6, &reg_total_,
                                      &reg_pags_, &reg_pag_);
      reg_sel_ = (reg_sel_fim_ && reg_n_) ? (uint8_t)(reg_n_ - 1) : 0;
      reg_fetch_ = REG_OK;
    } else {
      reg_fetch_ = REG_ERR;
    }
  }
  // OUVINDO: drena o DMA inteiro num unico write por tick (8 writes de
  // 512 bytes rendiam 66%: o overhead de cada registro TLS comia o loop)
  if (gravando_) {
    static int16_t buf[2048];  // teto de ~128 ms por tick (anti-prisao)
    size_t total = 0, n;
    while (total < 2048 && (n = mic::read(buf + total, 2048 - total)) > 0)
      total += n;
    if (total) {
      if (!voicecli::write((const uint8_t*)buf, total * sizeof(int16_t))) {
        // a conexao caiu no meio: corta e deixa o finish reportar o erro
        Serial.println("[voz] upload caiu no meio da gravacao");
        claude::maquina_evento(maq_, claude::EV_OK_SOLTA, now);
        encerra_gravacao(now);
        return;
      }
      uint32_t soma = 0;
      for (size_t i = 0; i < total; i++) soma += (uint16_t)abs(buf[i]);
      nivel_ = (uint16_t)(soma / total);
    }
    static uint32_t nivel_log_ = 0;  // 1x/s: mostra se o mic capta algo
    if ((int32_t)(now - nivel_log_) >= 0) {
      nivel_log_ = now + 1000;
      Serial.printf("[voz] nivel=%u\n", nivel_);
    }
  }
  // ping a cada 30 s: segura o keep-alive na borda e reconecta fora do
  // aperto (socket frio custava ~1,8 s de begin e comia o inicio da fala)
  if (!gravando_ && !finish_em_ && wifi::connected() &&
      (int32_t)(now - ping_em_) >= 0) {
    ping_em_ = now + 30000;
    voicecli::ping();
  }
  // timeouts da maquina: 15 s ouvindo (corta e processa), 60 s falando
  claude::Estado antes = maq_.st;
  if (claude::maquina_tick(maq_, now) && antes == claude::E_OUVINDO)
    encerra_gravacao(now);
  // PENSANDO: o finish agendado roda agora (bloqueia ate 20 s)
  if (finish_em_ && (int32_t)(now - finish_em_) >= 0) {
    finish_em_ = 0;
    static char resp[1536];
    int status = voicecli::finish(resp, sizeof(resp));
    uint32_t fim = millis();
    if (status == 200 &&
        claude::voz_parse(resp, nullptr, 0, texto_, sizeof(texto_))) {
      claude::maquina_evento(maq_, claude::EV_RESPOSTA, fim);
      comeca_fala(fim);
      ultima_min_ = agora_min();  // o carinho conta a partir de agora
      if (ultima_min_ > 0) {
        Preferences p;
        p.begin("claude", false);
        p.putLong64("ult", ultima_min_);
        p.end();
      }
    } else {
      snprintf(texto_, sizeof(texto_), "%s",
               tr(status == 422 ? STR_SAY_AGAIN : STR_NO_RESPONSE));
      claude::maquina_evento(maq_, claude::EV_FALHA, fim);
      comeca_fala(fim);
    }
  }
  // FALANDO: typewriter + bitspeech (um chirp por caractere revelado)
  if (maq_.st == claude::E_FALANDO && fala_viva_ &&
      (int32_t)(now - pausa_ate_) >= 0 && !buzzer::busy()) {
    claude::Tom t;
    size_t prox;
    if (claude::bitspeech_next(texto_, fala_pos_, t, &prox)) {
      if (t.freq)
        buzzer::beep(t.freq, t.dur_ms);
      else
        pausa_ate_ = now + t.dur_ms;
      fala_pos_ = prox;
      // a pagina acompanha o texto que ja apareceu
      char salvo = texto_[fala_pos_];
      texto_[fala_pos_] = '\0';
      uint16_t l = claude::linhas_total(texto_, kCols);
      texto_[fala_pos_] = salvo;
      if (l) pag_ = (uint8_t)((l - 1) / kRows);
      maq_.desde_ms = now;  // falando nao conta como "parado"
    } else {
      fala_viva_ = false;
    }
  }
}

// corpo + carinha por cima (o corpo nao tem rosto; as caras sao overlays)
static void desenha_pet(U8G2& g, const unsigned char* face, int dx) {
  g.drawXBMP(8 + dx, 11, CLAUDE_PET_W, CLAUDE_PET_H, claude_pet_bits);
  g.drawXBMP(8 + dx + 8, 11 + 1, FACE_NEUTRO_W, FACE_NEUTRO_H, face);
}

static void render_pet(U8G2& g, uint32_t now) {
  claude::Humor h = claude::humor(agora_min(), ultima_min_, hora_agora());
  // emocao em doses: aparece por 10 s quando o humor muda e volta pro idle;
  // re-aparece por 10 s a cada 45 s (sono nao e emocao — fica a noite toda)
  static claude::Humor ult_h = claude::H_NEUTRO;
  static uint32_t emo_t0 = 0;
  if (h != ult_h) {
    ult_h = h;
    emo_t0 = now;
  }
  if ((h == claude::H_FELIZ || h == claude::H_CARENTE) &&
      (now - emo_t0) % 45000 >= 10000)
    h = claude::H_NEUTRO;
  bool pisca = (now % 4200) < 160;  // piscadela de vez em quando
  const unsigned char* face =
      (h == claude::H_DORMINDO || pisca) ? face_dormindo_bits
      : h == claude::H_FELIZ             ? face_feliz_bits
      : h == claude::H_CARENTE           ? face_carente_bits
                                         : face_neutro_bits;
  desenha_pet(g, face, (now / 700) % 2 ? 1 : 0);  // balanca 1 px
  g.setFont(u8g2_font_3310_small);
  if (h == claude::H_DORMINDO) {
    g.drawStr(46, 16, "z");
    nokia_ui::text_bold(g, 50, 12, "Z");
  }
  rtc::DateTime dt;
  if (rtc::now(dt)) {  // relogio pequeno no canto
    char hh[6];
    hhmm_format(dt.hour, dt.min, hh);
    g.drawStr(83 - g.getStrWidth(hh), 7, hh);
  }
  // instrucao do push-to-talk com o mic no fim (fonte normal: precisa
  // caber), centrada fora da coluna da setinha do registro
  int w = (int)g.getUTF8Width(tr(STR_TALK));
  int x = (76 - (w + 2 + MINI_MIC_W)) / 2;
  if (x < 1) x = 1;
  g.drawUTF8(x, 47, tr(STR_TALK));
  g.drawXBMP(x + w + 2, 41, MINI_MIC_W, MINI_MIC_H, mini_mic_bits);
  // setinha pra baixo: o registro de conversa mora ali
  g.drawXBMP(78, 42, MINI_DOWN_W, MINI_DOWN_H, mini_down_bits);
}

static void render_ouvindo(U8G2& g, uint32_t now) {
  desenha_pet(g, face_neutro_bits, 0);
  g.setFont(u8g2_font_3310_small);  // titulo na fonte normal do sistema
  g.drawUTF8(2, 7, tr(STR_LISTENING));
  // mic ao lado do pet + ondas de som crescendo em 4 tempos (vazio, 1, 2, 3)
  g.drawXBMP(47, 14, ICON_MIC_W, ICON_MIC_H, icon_mic_bits);
  uint8_t f = (uint8_t)((now / 280) % 4);
  for (uint8_t r = 1; r <= f; r++)
    g.drawCircle(57, 17, r * 3, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_LOWER_RIGHT);
  // VU horizontal no lugar do softkey, com a altura da fonte do sistema
  int w = nivel_ / 28;
  if (w > 42) w = 42;
  g.drawFrame(20, 41, 44, 7);
  if (w) g.drawBox(21, 42, w, 5);
  // contador de segundos no canto, como o relogio do standby
  char s[8];
  snprintf(s, sizeof(s), "%us", (unsigned)((now - grava_t0_) / 1000));
  g.drawStr(83 - g.getStrWidth(s), 7, s);
}

static void render_pensando(U8G2& g, uint32_t now) {
  desenha_pet(g, face_neutro_bits, 0);
  nokia_ui::text_bold(g, 2, 7, tr(STR_THINKING));
  char dots[4];
  uint8_t n = (now / 350) % 4;
  memset(dots, '.', n);
  dots[n] = '\0';
  nokia_ui::text_bold(g, 48, 30, dots);
}

static void render_falando(U8G2& g) {
  char salvo = 0;
  if (fala_viva_) {  // typewriter: so o trecho ja "falado" aparece
    salvo = texto_[fala_pos_];
    texto_[fala_pos_] = '\0';
  }
  g.setFont(u8g2_font_3310_small);
  char linha[kCols * 3 + 1];  // folga pra UTF-8 multibyte
  for (uint8_t i = 0; i < kRows; i++) {
    if (!claude::linha_texto(texto_, (uint16_t)(pag_ * kRows + i), kCols,
                             linha, sizeof(linha)))
      break;
    g.drawUTF8(2, 8 + i * 8, linha);
  }
  if (fala_viva_) {
    texto_[fala_pos_] = salvo;
  } else {  // setinhas de paginacao, estilo jogo_view
    if (pag_ > 0) g.drawStr(79, 7, "^");
    if (pag_ + 1 < paginas()) g.drawStr(79, 47, "v");
  }
}

// registro em dois passos: lista de registros (data + hora do selecionado
// no header) e, no OK, a leitura do fluxo "EU:/CLAWD:" daquele par
static void render_registro(U8G2& g) {
  g.setFont(u8g2_font_3310_small);
  if (reg_fetch_ == REG_NONET) { nokia_ui::no_network(g); return; }
  if (reg_fetch_ == REG_PENDING) {
    g.drawUTF8(2, 24, tr(STR_SEARCHING));
    return;
  }
  if (reg_fetch_ == REG_ERR || reg_total_ == 0) {
    const char* s = tr(reg_fetch_ == REG_ERR ? STR_NO_RESPONSE
                                             : STR_NO_TALKS);
    g.drawUTF8(42 - (int)g.getUTF8Width(s) / 2, 24, s);
    return;
  }
  // header do registro selecionado: data | reloginho + hora | contador
  const claude::RegPar& sel = reg_itens_[reg_sel_];
  g.drawStr(2, 7, sel.d);
  int hw = (int)g.getStrWidth(sel.h);
  int hx = 42 - (MINI_CLOCK_W + 2 + hw) / 2;
  g.drawXBMP(hx, 1, MINI_CLOCK_W, MINI_CLOCK_H, mini_clock_bits);
  g.drawStr(hx + MINI_CLOCK_W + 2, 7, sel.h);
  char cont[12];
  snprintf(cont, sizeof(cont), "%u/%u",
           (unsigned)(reg_pag_ * 6 + reg_sel_ + 1), (unsigned)reg_total_);
  g.drawStr(83 - g.getStrWidth(cont), 7, cont);
  g.drawHLine(0, 9, 84);
  char linha[49];  // 16 colunas com folga pra UTF-8 multibyte
  if (reg_lendo_) {  // leitura: o fluxo do par, rolavel linha a linha
    for (uint8_t i = 0; i < 4; i++) {
      if (!claude::registro_linha(&reg_itens_[reg_sel_], 1, tr(STR_ME),
                                  reg_linha_ + i, linha, sizeof(linha),
                                  nullptr))
        break;
      g.drawUTF8(2, 17 + i * 8, linha);
    }
    if (reg_linha_ > 0) g.drawStr(79, 15, "^");
    if (reg_linha_ + 4 < reg_nlinhas_) g.drawStr(79, 47, "v");
    return;
  }
  // lista: janela de 4 registros, preview da fala, selecionado invertido
  uint8_t topo = reg_sel_ < 4 ? 0 : (uint8_t)(reg_sel_ - 3);
  for (uint8_t i = 0; i < 4 && topo + i < reg_n_; i++) {
    uint8_t idx = (uint8_t)(topo + i);
    claude::linha_texto(reg_itens_[idx].q, 0, 14, linha, sizeof(linha));
    int y = 11 + i * 9;
    if (idx == reg_sel_) {  // barra invertida, padrao das listas do sistema
      g.drawBox(0, y, 76, 9);
      g.setDrawColor(0);
    }
    g.drawUTF8(3, y + 8, linha);
    g.setDrawColor(1);
  }
  if (reg_sel_ > 0 || reg_pag_ > 0) g.drawStr(79, 17, "^");
  if (reg_sel_ + 1 < reg_n_ || reg_pag_ + 1 < reg_pags_)
    g.drawStr(79, 47, "v");
}

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  uint32_t now = millis();
  if (registro_) { render_registro(g); return; }
  switch (maq_.st) {
    case claude::E_PET: render_pet(g, now); break;
    case claude::E_OUVINDO: render_ouvindo(g, now); break;
    case claude::E_PENSANDO: render_pensando(g, now); break;
    case claude::E_FALANDO: render_falando(g); break;
  }
}

const App app_claude = {STR_APP_CLAUDE, on_enter, tick, input, on_exit,
                        render, icon_claude_bits};
