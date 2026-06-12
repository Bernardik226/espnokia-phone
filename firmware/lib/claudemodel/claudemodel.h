#pragma once
#include <stddef.h>
#include <stdint.h>

// Cérebro puro do pet do Claude (sem Arduino, testável no PC): humor pelo
// carinho, máquina de estados do push-to-talk, paginação da resposta,
// bitspeech (a "voz" no buzzer) e parse do JSON do server.
// O glue (app_claude) traduz RTC/NVS/botões/buzzer pra cá.
namespace claude {

// ---- humor (tudo em hora local; o RTC do projeto não tem epoch) ----
enum Humor : uint8_t { H_FELIZ, H_NEUTRO, H_CARENTE, H_DORMINDO };

int32_t dias_civis(uint16_t y, uint8_t m, uint8_t d);  // dias desde 1970-01-01
int64_t epoch_min(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mi);
// ultima_min <= 0 = nunca conversou (acordado vira NEUTRO);
// DORMINDO (23h–7h) passa por cima de tudo
Humor humor(int64_t agora_min, int64_t ultima_min, uint8_t hora_local);

}  // namespace claude
