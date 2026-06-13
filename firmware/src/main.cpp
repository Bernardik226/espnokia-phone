#include <Arduino.h>
#include <U8g2lib.h>
#include "alarm.h"
#include "copa_watch.h"
#include "pins.h"
#include "shell.h"
#include "sound.h"
#include "drivers/backlight.h"
#include "drivers/buzzer.h"
#include "drivers/buttons.h"
#include "drivers/mic.h"
#include "drivers/rtc.h"
#include "net/conn.h"
#include "net/wifi.h"
#include "net/ntp.h"
#include "ui/boot_anim.h"
#include "ui/goal_fx.h"
#include "ui/menu_view.h"
#include "apps/app_standby.h"
#include "apps/app_clock.h"
#include "apps/app_claude.h"
#include "apps/app_esportes.h"
#include "apps/app_weather.h"
#include "apps/app_tones.h"
#include "apps/app_settings.h"

static U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, PIN_LCD_CE, PIN_LCD_DC, PIN_LCD_RST);
static Shell shell;
static const App* kApps[] = {&app_clock,   &app_claude, &app_esportes,
                             &app_weather, &app_tones,  &app_settings};

void setup() {
  Serial.begin(115200);
  settings_load_lang();  // idioma salvo na NVS, antes do primeiro render
  backlight::init();  // PWM + nivel salvo na NVS
  u8g2.begin();
  u8g2.setContrast(140);
  buzzer::init();
  sound::init();  // volume + toque padrao salvos na NVS
  buttons::init();
  if (rtc::init()) {
    rtc::DateTime dt;
    rtc::now(dt);
    Serial.printf("[rtc] %04u-%02u-%02u %02u:%02u:%02u  %.2fC\n", dt.year,
                  dt.month, dt.day, dt.hour, dt.min, dt.sec, rtc::temperature());
  } else {
    Serial.println("[rtc] DS3231 ausente — relogio de videocassete");
  }
  conn::init();                       // chave do device + endereco do server (NVS)
  alarme::init();                     // recarrega aviso de jogo persistido
  copa_watch::init();                 // recarrega vigia de gols persistido
  wifi::init();                      // nao-bloqueante; conecta enquanto a animacao roda
  boot_anim_play(u8g2);              // logo NOKIA + maos + startup chime (segura ate o fim)
  shell.init(&app_standby, kApps, sizeof(kApps) / sizeof(kApps[0]));
}

void loop() {
  uint32_t now = millis();

  Button b; BtnEvent e;
  if (buttons::poll(now, b, e)) {
    if (!goal_fx::input(b, e)) {                    // overlays engolem teclas
      if (!alarme::input(b, e)) shell.input(b, e);
    }
    // bip de tecla so DEPOIS do input: a tecla que liga o mic (e qualquer
    // outra durante a gravacao) fica muda. Bipar antes segurava um tom de
    // 900 Hz ligado durante todo o handshake TLS do begin() — o tick que o
    // desligaria nao roda enquanto o input bloqueia.
    // (o auto-repeat de segurar UP/DOWN tambem fica mudo: 9 bips/s cansa)
    if (e == EV_PRESS && !mic::running() && !buttons::repeating())
      sound::play(sound::SND_KEY);
  }
  shell.tick(now);
  buzzer::tick(now);
  wifi::tick(now);
  ntp::tick(now);
  alarme::tick(now);
  copa_watch::tick(now);
  goal_fx::tick(now);

  static uint32_t last_render = 0;
  if (now - last_render >= 50) {                // ~20 fps
    last_render = now;
    u8g2.clearBuffer();
    if (shell.screen() == Shell::LAUNCHER) menu_view_draw(u8g2, shell);
    else if (shell.active() && shell.active()->on_render)
      shell.active()->on_render(&u8g2);
    if (alarme::active()) alarme::render(&u8g2);  // overlay por cima de tudo
    if (goal_fx::active()) goal_fx::render(&u8g2);  // gol por cima ate do alarme
    u8g2.sendBuffer();
  }
  delay(2);
}
