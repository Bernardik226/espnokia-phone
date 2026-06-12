#pragma once
#include <stddef.h>
#include <stdint.h>

// Cérebro puro do pet do Claude (sem Arduino, testável no PC): humor pelo
// carinho, máquina de estados do push-to-talk, paginação da resposta,
// bitspeech (a "voz" no buzzer) e parse do JSON do server.
// O glue (app_claude) traduz RTC/NVS/botões/buzzer pra cá.
namespace claude {

// ---- humor (tudo em hora local; o RTC do projeto não tem epoch) ----
enum Humor : uint8_t { H_FELIZ, H_NEUTRO, H_CARENTE, H_DORMINDO };

int32_t dias_civis(uint16_t y, uint8_t m, uint8_t d);  // dias desde 1970-01-01
int64_t epoch_min(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mi);
// ultima_min <= 0 = nunca conversou (acordado vira NEUTRO);
// DORMINDO (23h–7h) passa por cima de tudo
Humor humor(int64_t agora_min, int64_t ultima_min, uint8_t hora_local);

// ---- push-to-talk: PET -> OUVINDO (OK seguro) -> PENSANDO (OK solto) ->
// FALANDO (resposta ou erro) -> PET (C/EV_VOLTA ou 60 s parado) ----
enum Estado : uint8_t { E_PET, E_OUVINDO, E_PENSANDO, E_FALANDO };
enum Evento : uint8_t { EV_OK_APERTA, EV_OK_SOLTA, EV_RESPOSTA, EV_FALHA,
                        EV_VOLTA };

constexpr uint32_t kOuvindoMaxMs = 15000;  // corta a gravacao e processa
constexpr uint32_t kFalandoMaxMs = 60000;  // volta pro pet sozinho

struct Maquina {
  Estado st;
  uint32_t desde_ms;  // quando entrou no estado (o app renova ao paginar)
};

void maquina_init(Maquina& m, uint32_t now_ms);
bool maquina_evento(Maquina& m, Evento ev, uint32_t now_ms);  // true = mudou
// PENSANDO nao tem timeout proprio: o app DEVE disparar EV_FALHA se o HTTP
// nao retornar (timeout do client cobre isso).
bool maquina_tick(Maquina& m, uint32_t now_ms);               // timeouts

// ---- paginacao: quebra em palavras, colunas em caracteres UTF-8 ----
uint16_t linhas_total(const char* texto, uint8_t cols);
// copia a linha n (0-based) pra out; false se a linha nao existe
bool linha_texto(const char* texto, uint16_t n, uint8_t cols,
                 char* out, size_t out_len);

// ---- bitspeech: um chirp por caractere, deterministico ----
struct Tom {
  uint16_t freq;    // Hz; 0 = pausa (respiro)
  uint16_t dur_ms;
};
// Tom do caractere em texto[pos]; *prox recebe a posicao do proximo
// caractere (pula UTF-8 inteiro). false = fim do texto.
bool bitspeech_next(const char* texto, size_t pos, Tom& t, size_t* prox);

// ---- resposta do server: {"falei": "...", "resposta": "..."} ----
// falei pode ser nullptr (so interessa a resposta). false = JSON invalido
// ou sem "resposta".
bool voz_parse(const char* json, char* falei, size_t falei_len,
               char* resposta, size_t resp_len);

// ---- registro de conversa: GET /claude/registro paginado ----
struct RegPar {
  char q[164];   // MAX_Q_BYTES do server + nul (a "pergunta" da pessoa)
  char r[284];   // MAX_R_BYTES do server + nul (a resposta do Clawd)
};
// devolve quantos itens leu (0 em JSON invalido); total/pags/pag por
// referencia (zerados em lixo, pra nao herdar valor velho)
uint8_t registro_parse(const char* json, RegPar* itens, uint8_t max,
                       uint16_t* total, uint8_t* pags, uint8_t* pag);

}  // namespace claude
