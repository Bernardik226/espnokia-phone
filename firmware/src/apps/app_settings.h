#pragma once
#include "app.h"

extern const App app_settings;

// preferencia "Data e hora > Sincronizar" (NVS); a F2 le isso pra decidir
// se acerta o RTC via NTP quando o WiFi conectar
bool settings_ntp_sync_enabled();
