#pragma once
#include <stdint.h>

// F1 não tem rede: relógio "anda" a partir de 12:00 no boot (estilo
// videocassete). O ':' pisca a cada segundo, como nos Nokia reais.
// out deve ter >= 6 bytes ("HH:MM\0"); colon_on diz se o ':' aparece.
void clock_format(uint32_t ms_since_boot, char out[6], bool* colon_on);
