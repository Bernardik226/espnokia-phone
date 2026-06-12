#pragma once
#include <stddef.h>
#include <stdint.h>

// INMP441 no I2S0 (RX): 16 kHz mono, canal esquerdo (L/R do modulo no GND).
// O mic manda 24 bits uteis num slot de 32; read() ja entrega PCM16 LE
// pronto pro upload. Nao bloqueia (timeout 0) — o nivel (RMS) e por conta
// do app, o driver so captura.
namespace mic {
bool init();                            // instala o driver (uma vez)
void start();                           // limpa o DMA e comeca a ouvir
size_t read(int16_t* buf, size_t max);  // amostras copiadas (0 = nada ainda;
                                        // max e limitado a 256 por chamada)
void stop();
bool running();  // captura ativa? (outros modulos adiam trabalho bloqueante)
}  // namespace mic
