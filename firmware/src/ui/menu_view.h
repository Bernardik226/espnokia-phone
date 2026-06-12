#pragma once
#include <U8g2lib.h>
#include "shell.h"

// Menu do 3310: carrossel um-item-por-tela (header + icone 24x24 central +
// scrollbar proporcional na direita + softkey "Selecionar").
// A forma generica serve qualquer carrossel de apps (ex.: categoria
// Esportes); a do Shell e o launcher de sempre.
void menu_view_draw(U8G2& g, const App& a, uint8_t n, uint8_t cur);
void menu_view_draw(U8G2& g, const Shell& shell);
