#pragma once
#include <stdint.h>
#include "btnlogic.h"
#include "i18n.h"

// Avisos do sistema: um alarme agendado por vez (persistido na NVS, sobrevive
// reboot) + um timer regressivo (volatil) + disparo imediato via trigger().
// Ao disparar vira overlay fullscreen com o titulo piscando e o toque padrao
// em loop ate OK/C. Enquanto ativo, input() engole todas as teclas (o main
// consulta antes de repassar pro shell). E a infra de notificacao do aparelho:
// qualquer app levanta um aviso com trigger(titulo, label).
namespace alarme {
void init();
void set(StrId title, uint8_t dia, uint8_t mes, uint8_t h, uint8_t m,
         const char* label);
void clear();
bool armed(uint8_t dia, uint8_t mes, uint8_t h, uint8_t m);  // aviso deste jogo?
bool armed_at(uint8_t& h, uint8_t& m);  // ha alarme armado? devolve a hora
void trigger(StrId title, const char* label);  // overlay + toque, na hora

// timer regressivo: conta por millis, dispara trigger(STR_TIME_UP)
void timer_start(uint16_t minutos);   // atalho de minutos
void timer_start_s(uint32_t segundos);
void timer_cancel();
uint32_t timer_left_s();            // 0 = nenhum timer rodando

bool active();                      // overlay tocando agora
void tick(uint32_t now_ms);
bool input(Button b, BtnEvent e);   // true = consumiu (overlay ativo)
void render(void* gfx);             // desenhar DEPOIS do render do app
}  // namespace alarme
