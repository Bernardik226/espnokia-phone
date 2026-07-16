#pragma once
#include <stdint.h>

// backlight do 5110 em PWM (LEDC) com persistencia em NVS
namespace backlight {
constexpr uint8_t kLevels = 3;  // 0=desligada 1=media 2=alta
void init();                    // restaura nivel salvo e liga o PWM
void apply(uint8_t l);          // muda o duty na hora (preview, nao persiste)
void save(uint8_t l);           // aplica e grava na NVS
uint8_t level();                // nivel persistido
}  // namespace backlight
