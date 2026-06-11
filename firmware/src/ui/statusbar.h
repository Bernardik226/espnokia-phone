#pragma once
#include <U8g2lib.h>

// Barra de 8px no topo: [wifi] ... hora ... [bateria]
void statusbar_draw(U8G2& g, const char* clock_str, bool wifi_on);
