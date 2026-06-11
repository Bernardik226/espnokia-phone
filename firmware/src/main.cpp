#include <Arduino.h>
#include <U8g2lib.h>
#include "pins.h"
#include "drivers/buzzer.h"

static const char* kNokiaTune =
    "Nokia:d=4,o=5,b=225:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a";

U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, PIN_LCD_CE, PIN_LCD_DC, PIN_LCD_RST);

void setup() {
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);
  u8g2.begin();
  u8g2.setContrast(140);  // ajustar 120-160 conforme o módulo
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(8, 20, "espnokia");
  u8g2.drawFrame(0, 0, 84, 48);
  u8g2.sendBuffer();
  buzzer::init();
  buzzer::play(kNokiaTune);
}
void loop() {
  buzzer::tick(millis());
  delay(2);
}
