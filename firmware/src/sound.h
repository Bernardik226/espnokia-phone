#pragma once
#include <stdint.h>

// Sons do sistema: catalogo de bips + toque padrao, com volume unico (NVS).
// Camada acima do driver buzzer — quem quer "som de erro" pede SND_ERROR e
// nao precisa saber de RTTTL nem de duty PWM.
namespace sound {
enum Snd : uint8_t {
  SND_KEY,      // tecla (respeita um toque em andamento)
  SND_CONFIRM,  // acao salva/confirmada
  SND_ERROR,    // acao recusada/falhou
  SND_NOTIFY,   // aviso discreto (estilo SMS)
  SND_ALERT,    // aviso insistente (gol, alarme chegando)
};
void init();                     // carrega volume + toque padrao da NVS
void play(Snd s);
void ringtone();                 // toca o toque escolhido no app Toques
uint8_t volume();                // 0=baixo 1=medio 2=alto
void set_volume(uint8_t lvl);    // aplica no buzzer e persiste
bool muted();                    // mute geral do sistema
void set_muted(bool m);          // aplica no buzzer e persiste
uint8_t ringtone_idx();          // indice no catalogo do app Toques
void set_ringtone(uint8_t idx);  // persiste
}  // namespace sound
