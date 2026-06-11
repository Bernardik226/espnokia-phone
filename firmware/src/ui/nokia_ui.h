#pragma once
#include <U8g2lib.h>

// Idioma visual do 3310: bold simulado (draw duplo deslocado 1px) e
// softkey label centralizado na base da tela.
namespace nokia_ui {
// bold simulado na fonte corrente; (x, y) = baseline, como drawStr
void text_bold(U8G2& g, int x, int y, const char* s);
// bold centrado horizontalmente (tela de 84px); y = baseline
void text_bold_center(U8G2& g, int y, const char* s);
// label de softkey centrado na base (sempre nokiafc22 bold, baseline 47)
void softkey(U8G2& g, const char* label);
}  // namespace nokia_ui
