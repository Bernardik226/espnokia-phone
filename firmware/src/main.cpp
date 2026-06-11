#include <Arduino.h>
#include <U8g2lib.h>
#include "pins.h"
#include "shell.h"
#include "drivers/buzzer.h"
#include "drivers/buttons.h"
#include "ui/boot_anim.h"
#include "ui/launcher_view.h"
#include "apps/app_standby.h"
#include "apps/app_clock.h"
#include "apps/app_tones.h"
#include "apps/app_about.h"

static U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, PIN_LCD_CE, PIN_LCD_DC, PIN_LCD_RST);
static Shell shell;
static const App* kApps[] = {&app_clock, &app_tones, &app_about};

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);
  u8g2.begin();
  u8g2.setContrast(140);
  buzzer::init();
  buttons::init();
  boot_anim_play(u8g2);              // logo NOKIA + maos + startup chime (segura ate o fim)
  shell.init(&app_standby, kApps, sizeof(kApps) / sizeof(kApps[0]));
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
    u8g2.clearBuffer();
    if (shell.screen() == Shell::LAUNCHER) launcher_view_draw(u8g2, shell);
    else if (shell.active() && shell.active()->on_render)
      shell.active()->on_render(&u8g2);
    u8g2.sendBuffer();
  }
  delay(2);
}
