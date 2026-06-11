#include "app_standby.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "clockfmt.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// nivel de sinal 0..4 — fixo em 0 ate a F2 ligar o WiFi (vira RSSI real)
static uint8_t wifi_level() { return 0; }

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char hhmm[6]; bool colon;
  clock_format(millis(), hhmm, &colon);
  if (!colon) hhmm[2] = ' ';

  g.setFont(u8g2_font_3310_small);
  // hora bold no topo direito (como no 3310)
  nokia_ui::text_bold(g, 82 - (int)g.getStrWidth(hhmm), 8, hhmm);
  // marca do projeto no lugar do nome da operadora: emblema eN + wordmark
  g.drawXBMP(24, 9, ESPNOKIA_EMBLEM_W, ESPNOKIA_EMBLEM_H, espnokia_emblem_bits);
  g.drawXBMP(2, 31, ESPNOKIA_LOGO_W, ESPNOKIA_LOGO_H, espnokia_logo_bits);
  // coluna de sinal na esquerda: icone WiFi no topo, barras abaixo
  g.drawXBMP(0, 0, ICON_WIFI_W, ICON_WIFI_H, icon_wifi_bits);
  for (uint8_t i = 0; i < wifi_level(); i++)            // i=0 e a barra de baixo
    g.drawBox(0, 38 - i * 7, 3 + i, 5);                 // largura cresce pra cima
  nokia_ui::softkey(g, "Menu");
}

const App app_standby = {"Standby", nullptr, nullptr, nullptr, nullptr, render, nullptr};
