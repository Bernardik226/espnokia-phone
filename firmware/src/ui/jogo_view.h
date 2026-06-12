#pragma once
#include <U8g2lib.h>
#include "copamodel.h"

// Detail de um jogo em 3 paginas, compartilhado entre Copa e Futebol:
// 0 placar/data + info + AO VIVO/aviso, 1 nome das equipes (n1/n2 do server;
// selecao sem n1 cai pra tabela de traducao), 2 autores dos gols + estadio.
// tem_aviso troca a softkey (Avisar/Tirar aviso) e mostra "Aviso ON".
void jogo_view_detail(U8G2& g, const CopaJogo& j, uint8_t pag, bool tem_aviso);
