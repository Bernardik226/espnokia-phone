#include "app_standby.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "clockfmt.h"
#include "drivers/rtc.h"
#include "i18n.h"
#include "net/wifi.h"
#include "timeprefs.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"
#include "ui/wallpaper.h"

// nivel de sinal 0..4 vindo do RSSI real
static uint8_t wifi_level() { return wifi::level(); }

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  // hora do RTC quando presente; sem ele, relogio de videocassete
  char hhmm[6];
  bool colon = ((millis() / 1000) % 2) == 0;
  bool pm = false;
  rtc::DateTime dt;
  bool tem = rtc::now(dt);
  if (tem) hhmm_format12(dt.hour, dt.min, timeprefs::fmt24(), hhmm, &pm);
  else clock_format(millis(), hhmm, &colon);

  g.setFont(u8g2_font_3310_small);
  // sufixo AM/PM (só em 12h com RTC), fonte pequena colada à direita
  const char* ap = (tem && !timeprefs::fmt24()) ? (pm ? "PM" : "AM") : nullptr;
  int apw = ap ? (int)g.getStrWidth(ap) + 2 : 0;
  int rx = 82 - apw;                                 // borda direita da hora
  // hora bold no topo direito (como no 3310). Pra o ':' piscar LIMPO (apaga
  // por inteiro, sem sobrar meio ponto), HH e MM ficam em posicao fixa e o
  // ':' so e desenhado quando aceso — nada de apagar por cima.
  char hh[3] = {hhmm[0], hhmm[1], '\0'};
  char hhc[4] = {hhmm[0], hhmm[1], ':', '\0'};
  char mm[3] = {hhmm[3], hhmm[4], '\0'};
  int tx = rx - nokia_ui::bold_width(g, hhmm) - 1;   // -1 compensa a folga extra abaixo
  nokia_ui::text_bold(g, tx, 8, hh);
  nokia_ui::text_bold(g, tx + nokia_ui::bold_width(g, hhc) + 1, 8, mm);  // +1px afasta HH<->MM
  // ':' +1px pra não encostar nas horas, no vão entre HH e MM
  if (colon) nokia_ui::text_bold(g, tx + nokia_ui::bold_width(g, hh) + 1, 8, ":");
  if (ap) g.drawStr(rx + 2, 8, ap);
  // data DD/MM à esquerda da hora (Config > Data e hora > Mostrar data)
  if (tem && timeprefs::show_date()) {
    char ds[6];
    snprintf(ds, sizeof(ds), "%02u/%02u", dt.day, dt.month);
    g.drawStr(tx - (int)g.getStrWidth(ds) - 4, 8, ds);
  }
  // fundo escolhido pelo usuário (Config > Papel de parede); relógio/sinal ficam por cima
  wallpaper::draw(g);
  // coluna de sinal na esquerda: icone WiFi + barras do RSSI real.
  // sem rede conectada nao aparece nada, como celular sem chip
  if (wifi::connected()) {
    g.drawXBMP(0, 0, ICON_WIFI_W, ICON_WIFI_H, icon_wifi_bits);
    for (uint8_t i = 0; i < wifi_level(); i++)          // i=0 e a barra de baixo
      g.drawBox(0, 38 - i * 7, 3 + i, 5);               // largura cresce pra cima
  }
  nokia_ui::softkey(g, tr(STR_MENU));
}

const App app_standby = {STR_NONE, nullptr, nullptr, nullptr, nullptr, render, nullptr};
