#pragma once
#include "copamodel.h"

// Par de avisos de um jogo, compartilhado entre Copa e Futebol: inicio via
// alarme (so se o jogo ainda nao comecou) + gols via vigia em background.
// live_path diz onde o vigia consulta o placar ("/copa/live" ou
// "/futebol/live?liga=X"). Ambos persistem na NVS.
bool jogo_aviso_armado(const CopaJogo& j);
// liga/desliga o par; retorna o estado novo (true = avisos ligados)
bool jogo_aviso_toggle(const CopaJogo& j, const char* live_path);
