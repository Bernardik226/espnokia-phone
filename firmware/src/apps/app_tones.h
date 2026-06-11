#pragma once
#include "app.h"
extern const App app_tones;

// catalogo de toques de fabrica: o sound:: busca aqui o toque padrao
uint8_t tones_count();
const char* tones_name(uint8_t i);
const char* tones_rtttl(uint8_t i);
