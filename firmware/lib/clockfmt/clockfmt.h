#pragma once
#include <stdint.h>

// Formata "HH:MM\0" em out (>= 6 bytes) a partir de hora/minuto conhecidos
// (ex.: vindos do RTC).
void hhmm_format(uint8_t hour, uint8_t minute, char out[6]);

// Fallback sem RTC: relógio "anda" a partir de 12:00 no boot (estilo
// videocassete). colon_on diz se o ':' aparece (pisca a cada segundo,
// como nos Nokia reais).
void clock_format(uint32_t ms_since_boot, char out[6], bool* colon_on);
