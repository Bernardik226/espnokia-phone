#pragma once
#include <stdint.h>
#include "btnlogic.h"

// Efeito de gol fullscreen: a bola da Copa atravessa a tela girando
// (horizontal ou diagonal, sorteio 50/50 no RNG de hardware) e depois entra
// "GOL!" piscando com o placar e o autor. O toque padrao toca em loop SO
// enquanto o efeito esta na tela. Overlay igual ao do alarme: o main
// renderiza por cima do app e input() engole as teclas (OK/C pulam).
namespace goal_fx {
// placar tipo "MEX 2x0 RSA"; autor tipo "R. Jimenez 67'" (pode ser "").
// Ignora se ja ativo ou se este placar acabou de ser mostrado — o detail
// aberto e o vigia em background detectam o mesmo gol sem duplicar.
void start(const char* placar, const char* autor);
bool active();
void tick(uint32_t now_ms);
bool input(Button b, BtnEvent e);  // true = consumiu (efeito na tela)
void render(void* gfx);            // desenhar DEPOIS do render do app
}  // namespace goal_fx
