#pragma once

// Preferências de exibição de hora, compartilhadas entre a tela inicial
// (app_standby), o app relógio (app_clock) e as configs (app_settings).
// Persistem na NVS (namespace "espnokia"); carga preguiçosa no 1º uso.
namespace timeprefs {
void init();               // opcional: força a carga no boot (idempotente)

bool fmt24();              // true = 24h (padrão); false = 12h com AM/PM
void set_fmt24(bool v);    // persiste na hora

bool show_date();          // false (padrão) = só a hora no menu inicial
void set_show_date(bool v);
}  // namespace timeprefs
