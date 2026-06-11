#include "app_standby.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "clockfmt.h"
#include "drivers/rtc.h"
#include "net/wifi.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// nivel de sinal 0..4 vindo do RSSI real
static uint8_t wifi_level() { return wifi::level(); }

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  // hora do RTC quando presente; sem ele, relogio de videocassete
  char hhmm[6];
  bool colon = ((millis() / 1000) % 2) == 0;
  rtc::DateTime dt;
  if (rtc::now(dt)) hhmm_format(dt.hour, dt.min, hhmm);
  else clock_format(millis(), hhmm, &colon);

  g.setFont(u8g2_font_3310_small);
  // hora bold no topo direito (como no 3310); posicao fixa medida na bold
  // pra nao pular quando o colon pisca: desenha "HH:MM" e apaga so o ':'
  int tx = 82 - nokia_ui::bold_width(g, hhmm);
  nokia_ui::text_bold(g, tx, 8, hhmm);
  if (!colon) {
    char hh[3] = {hhmm[0], hhmm[1], '\0'};
    g.setDrawColor(0);
    g.drawBox(tx + nokia_ui::bold_width(g, hh), 0, nokia_ui::bold_width(g, ":"), 9);
    g.setDrawColor(1);
  }
  // marca do projeto no lugar do nome da operadora: emblema eN + wordmark
  g.drawXBMP(24, 9, ESPNOKIA_EMBLEM_W, ESPNOKIA_EMBLEM_H, espnokia_emblem_bits);
  g.drawXBMP(2, 31, ESPNOKIA_LOGO_W, ESPNOKIA_LOGO_H, espnokia_logo_bits);
  // coluna de sinal na esquerda: icone WiFi + barras do RSSI real.
  // sem rede conectada nao aparece nada, como celular sem chip
  if (wifi::connected()) {
    g.drawXBMP(0, 0, ICON_WIFI_W, ICON_WIFI_H, icon_wifi_bits);
    for (uint8_t i = 0; i < wifi_level(); i++)          // i=0 e a barra de baixo
      g.drawBox(0, 38 - i * 7, 3 + i, 5);               // largura cresce pra cima
  }
  nokia_ui::softkey(g, "Menu");
}

const App app_standby = {"Standby", nullptr, nullptr, nullptr, nullptr, render, nullptr};
