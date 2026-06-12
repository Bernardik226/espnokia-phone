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
static size_t fala_pos_ = 0;      // typewriter/bitspeech: posicao no texto
static uint32_t pausa_ate_ = 0;   // respiro do bitspeech (freq 0)
static bool fala_viva_ = false;   // typewriter rodando (so na 1a passada)
static uint8_t pag_ = 0;

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
  mic::init();
}

static void on_exit() {
  if (gravando_) {
    mic::stop();
    gravando_ = false;
  }
  voicecli::abort();
  finish_em_ = 0;
}

static bool input(Button b, BtnEvent e) {
  uint32_t now = millis();
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
      grava_t0_ = now;
      nivel_ = 0;
      claude::maquina_evento(maq_, claude::EV_OK_APERTA, now);
      return true;
    }
    return false;  // C nao consumido: o shell fecha o app
  }
  if (maq_.st == claude::E_OUVINDO) {
    if (e == EV_RELEASE && b == BTN_OK) {
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
  // OUVINDO: despeja o mic no upload e mede o nivel pro VU
  if (gravando_) {
    int16_t buf[256];
    size_t n = mic::read(buf, 256);
    if (n) {
      if (!voicecli::write((const uint8_t*)buf, n * sizeof(int16_t))) {
        // a conexao caiu no meio: corta e deixa o finish reportar o erro
        claude::maquina_evento(maq_, claude::EV_OK_SOLTA, now);
        encerra_gravacao(now);
        return;
      }
      uint32_t soma = 0;
      for (size_t i = 0; i < n; i++) soma += (uint16_t)abs(buf[i]);
      nivel_ = (uint16_t)(soma / n);
    }
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
  nokia_ui::softkey(g, tr(STR_TALK));
}

static void render_ouvindo(U8G2& g, uint32_t now) {
  desenha_pet(g, face_neutro_bits, 0);
  nokia_ui::text_bold(g, 2, 7, tr(STR_LISTENING));
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
  g.setFont(u8g2_font_3310_small);
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

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  uint32_t now = millis();
  switch (maq_.st) {
    case claude::E_PET: render_pet(g, now); break;
    case claude::E_OUVINDO: render_ouvindo(g, now); break;
    case claude::E_PENSANDO: render_pensando(g, now); break;
    case claude::E_FALANDO: render_falando(g); break;
  }
}

const App app_claude = {STR_APP_CLAUDE, on_enter, tick, input, on_exit,
                        render, icon_claude_bits};
