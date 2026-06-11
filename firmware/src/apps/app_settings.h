#pragma once
#include "app.h"

extern const App app_settings;

// preferencia "Data e hora > Sincronizar" (NVS); a F2 le isso pra decidir
// se acerta o RTC via NTP quando o WiFi conectar
bool settings_ntp_sync_enabled();

// carrega o idioma salvo na NVS pra lib i18n; chamar no boot antes do
// primeiro render (senao a primeira tela sai em portugues por um frame)
void settings_load_lang();
