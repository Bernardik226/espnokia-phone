#pragma once
#include <U8g2lib.h>

// Idioma visual do 3310: negrito real (fonte 3310 small bold, fonts3310.h)
// e softkey label centralizado na base da tela.
namespace nokia_ui {
// negrito 3310; (x, y) = baseline, como drawStr; preserva a fonte corrente
void text_bold(U8G2& g, int x, int y, const char* s);
// campo selecionado do editor: caixa preenchida + texto em cor invertida
// (x, baseline) = onde drawStr(s) desenharia; a caixa cerca esse retangulo
void inv_str(U8G2& g, int x, int baseline, const char* s);
// negrito centrado horizontalmente (tela de 84px); y = baseline
void text_bold_center(U8G2& g, int y, const char* s);
// largura do texto na fonte negrito (pra alinhar sem desenhar)
int bold_width(U8G2& g, const char* s);
// label de softkey centrado na base (negrito 3310, baseline 47)
void softkey(U8G2& g, const char* label);
// tela padrao dos apps com internet sem rede: wifi cortado + instrucao
// (titulo e softkey ficam por conta do app)
void no_network(U8G2& g);
// poda s in place ate caber em max_w, marcando o corte com '.' — sem partir
// caractere UTF-8 no meio (estilo 3310: "Premier League" → "Premier Lea.")
void poda(U8G2& g, char* s, int max_w);
}  // namespace nokia_ui
