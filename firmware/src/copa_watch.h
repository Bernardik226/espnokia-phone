#pragma once
#include <stdint.h>

// Vigia de gols em background, persistido na NVS (sobrevive a corte de
// energia, igual ao alarme): marque o aviso de um jogo no app da Copa e o
// aparelho avisa o gol em QUALQUER tela — standby incluso. Na janela do jogo
// consulta /copa/live a cada 60 s; placar subiu → goal_fx com placar e autor.
// O primeiro fetch so estabelece a baseline (armar no meio do jogo nao
// dispara os gols que ja sairam). Jogo fora do feed ou janela estourada →
// desarma sozinho.
namespace copa_watch {
void init();  // recarrega da NVS no boot
void arm(const char* t1, const char* t2, uint8_t dia, uint8_t mes,
         uint8_t h, uint8_t m);
void disarm();
bool armed(const char* t1, const char* t2);  // este jogo esta vigiado?
void tick(uint32_t now_ms);
}  // namespace copa_watch
