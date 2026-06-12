#pragma once
#include <stdint.h>
#include "i18n.h"

// Nome da selecao no idioma corrente a partir do codigo FIFA ("BRA" →
// "Brasil"). Codigo desconhecido (placeholder do mata-mata) volta o proprio
// codigo. Tabela em flash, pura (testavel no env native). Nomes longos
// abreviados a moda dos celulares da epoca (tela de ~16 caracteres).
const char* team_name(const char* fifa_code);
uint8_t team_count();  // quantas selecoes a tabela conhece (48 na copa)
