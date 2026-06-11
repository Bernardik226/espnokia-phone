#include "backlight.h"
#include <Arduino.h>
#include <Preferences.h>
#include "pins.h"

// canal 2 = timer LEDC proprio (canal 0 e do buzzer, que varia a frequencia)
static const uint8_t kCh = 2;
static const uint8_t kDuty[backlight::kLevels] = {0, 90, 255};
static const char* kNames[backlight::kLevels] = {"Desligada", "Media", "Alta"};
static uint8_t cur_ = 2;
static Preferences prefs_;

void backlight::init() {
  prefs_.begin("espnokia");
  cur_ = prefs_.getUChar("bl", 2);
  if (cur_ >= kLevels) cur_ = kLevels - 1;
  ledcSetup(kCh, 5000, 8);
  ledcAttachPin(PIN_LCD_BL, kCh);
  ledcWrite(kCh, kDuty[cur_]);
}

void backlight::apply(uint8_t l) {
  if (l < kLevels) ledcWrite(kCh, kDuty[l]);
}

void backlight::save(uint8_t l) {
  if (l >= kLevels) return;
  cur_ = l;
  ledcWrite(kCh, kDuty[l]);
  prefs_.putUChar("bl", l);
}

uint8_t backlight::level() { return cur_; }
const char* backlight::name(uint8_t l) { return l < kLevels ? kNames[l] : ""; }
