#include <Arduino.h>
#include <U8g2lib.h>
#include "pins.h"
#include "shell.h"
#include "drivers/backlight.h"
#include "drivers/buzzer.h"
#include "drivers/buttons.h"
#include "drivers/rtc.h"
#include "ui/boot_anim.h"
#include "ui/menu_view.h"
#include "apps/app_standby.h"
#include "apps/app_clock.h"
#include "apps/app_weather.h"
#include "apps/app_tones.h"
#include "apps/app_settings.h"

static U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, PIN_LCD_CE, PIN_LCD_DC, PIN_LCD_RST);
static Shell shell;
static const App* kApps[] = {&app_clock, &app_weather, &app_tones, &app_settings};

void setup() {
  Serial.begin(115200);
  backlight::init();  // PWM + nivel salvo na NVS
  u8g2.begin();
  u8g2.setContrast(140);
  buzzer::init();
  buttons::init();
  if (rtc::init()) {
    rtc::DateTime dt;
    rtc::now(dt);
    Serial.printf("[rtc] %04u-%02u-%02u %02u:%02u:%02u  %.2fC\n", dt.year,
                  dt.month, dt.day, dt.hour, dt.min, dt.sec, rtc::temperature());
  } else {
    Serial.println("[rtc] DS3231 ausente — relogio de videocassete");
  }
  boot_anim_play(u8g2);              // logo NOKIA + maos + startup chime (segura ate o fim)
  shell.init(&app_standby, kApps, sizeof(kApps) / sizeof(kApps[0]));
}

void loop() {
  uint32_t now = millis();

  Button b; BtnEvent e;
  if (buttons::poll(now, b, e)) {
    if (e == EV_PRESS) buzzer::beep(900, 90);  // keypad beep DCT3: ~900 Hz / ~90ms medidos de 3310 real
    shell.input(b, e);
  }
  shell.tick(now);
  buzzer::tick(now);

  static uint32_t last_render = 0;
  if (now - last_render >= 50) {                // ~20 fps
    last_render = now;
    u8g2.clearBuffer();
    if (shell.screen() == Shell::LAUNCHER) menu_view_draw(u8g2, shell);
    else if (shell.active() && shell.active()->on_render)
      shell.active()->on_render(&u8g2);
    u8g2.sendBuffer();
  }
  delay(2);
}
