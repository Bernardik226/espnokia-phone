#pragma once
#include <U8g2lib.h>
#include "shell.h"

// Menu do 3310: carrossel um-item-por-tela (header + icone 24x24 central +
// scrollbar proporcional na direita + softkey "Selecionar").
void menu_view_draw(U8G2& g, const Shell& shell);
