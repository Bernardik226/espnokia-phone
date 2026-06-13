#include "app_weather.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "drivers/rtc.h"
#include "i18n.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Clima: por enquanto a temperatura ambiente vinda do DS3231 (o RTC tem um
// termometro de bordo, resolucao 0.25C). Previsao via API de tempo entra
// quando o WiFi ligar (F2+).
static float temp_ = 0.0f;
static uint32_t last_read_ = 0;

static void on_enter() {
  temp_ = rtc::temperature();
  last_read_ = millis();
}

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_3310_small);
  nokia_ui::text_bold_center(g, 8, tr(STR_APP_WEATHER));
  if (rtc::present()) {
    uint32_t ms = millis();
    if (ms - last_read_ >= 5000) {  // o DS3231 so converte a cada ~64s
      temp_ = rtc::temperature();
      last_read_ = ms;
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%.1f", temp_);
    g.setFont(u8g2_font_VCR_OSD_tn);  // digitos grandes, como no app Relogio
    int w = (int)g.getStrWidth(buf);
    int x = 42 - (w + 12) / 2;        // +12 = grau + C a direita
    g.drawStr(x, 30, buf);
    g.drawCircle(x + w + 4, 16, 2);   // simbolo de grau (fonte nao tem)
    g.setFont(u8g2_font_3310_small);
    nokia_ui::text_bold(g, x + w + 8, 23, "C");
    const char* sub = tr(STR_INT_SENSOR);
    g.drawUTF8(42 - (int)g.getUTF8Width(sub) / 2, 40, sub);
  } else {
    g.drawUTF8(2, 24, tr(STR_NO_SENSOR));
  }
  nokia_ui::softkey(g, tr(STR_BACK));
}

// animacao de selecao: o sol recolhe os raios e cintila duas vezes
static const unsigned char* const kAnim[] = {
    icon_weather_f1_bits, icon_weather_bits, icon_weather_f1_bits, nullptr};
const App app_weather = {STR_APP_WEATHER, on_enter, nullptr, nullptr, nullptr, render,
                         icon_weather_bits, kAnim};
