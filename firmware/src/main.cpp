#include <Arduino.h>
#include <U8g2lib.h>
#include "pins.h"
#include "shell.h"
#include "clockfmt.h"
#include "drivers/buzzer.h"
#include "drivers/buttons.h"
#include "ui/statusbar.h"
#include "ui/launcher_view.h"
#include "apps/app_clock.h"
#include "apps/app_tones.h"
#include "apps/app_about.h"

static U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, PIN_LCD_CE, PIN_LCD_DC, PIN_LCD_RST);
static Shell shell;
static const App* kApps[] = {&app_tones, &app_about};
static const char* kNokiaTune =
    "Nokia:d=4,o=5,b=225:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a";

static void splash() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(42 - (int)u8g2.getStrWidth("espnokia") / 2, 22, "espnokia");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(42 - (int)u8g2.getStrWidth("phone") / 2, 32, "phone");
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);
  u8g2.begin();
  u8g2.setContrast(140);
  buzzer::init();
  buttons::init();
  splash();
  buzzer::play(kNokiaTune);          // não-bloqueante: splash fica até o fim
  uint32_t t0 = millis();
  while (buzzer::busy() && millis() - t0 < 4000) { buzzer::tick(millis()); delay(2); }
  shell.init(&app_clock, kApps, sizeof(kApps) / sizeof(kApps[0]));
}

void loop() {
  uint32_t now = millis();

  Button b; BtnEvent e;
  if (buttons::poll(now, b, e)) {
    if (e == EV_PRESS) buzzer::beep(1000, 60);  // keypad beep estilo DCT3 (ajuste fino na bancada)
    shell.input(b, e);
  }
  shell.tick(now);
  buzzer::tick(now);

  static uint32_t last_render = 0;
  if (now - last_render >= 50) {                // ~20 fps
    last_render = now;
    char hhmm[6]; bool colon;
    clock_format(now, hhmm, &colon);
    if (!colon) hhmm[2] = ' ';

    u8g2.clearBuffer();
    statusbar_draw(u8g2, hhmm, false);          // wifi off na F1
    if (shell.screen() == Shell::LAUNCHER) launcher_view_draw(u8g2, shell);
    else if (shell.active() && shell.active()->on_render)
      shell.active()->on_render(&u8g2);
    u8g2.sendBuffer();
  }
  delay(2);
}
