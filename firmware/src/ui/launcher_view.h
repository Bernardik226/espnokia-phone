#pragma once
#include <U8g2lib.h>
#include "shell.h"

// Lista vertical estilo 3310: até 4 itens, selecionado em barra invertida.
void launcher_view_draw(U8G2& g, const Shell& shell);
