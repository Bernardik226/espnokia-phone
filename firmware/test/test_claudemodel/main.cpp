#include <unity.h>
#include <string.h>
#include "claudemodel.h"

using namespace claude;

// ---- humor ----
void test_dias_civis_ancoras() {
  TEST_ASSERT_EQUAL_INT32(0, dias_civis(1970, 1, 1));
  TEST_ASSERT_EQUAL_INT32(1, dias_civis(1970, 1, 2));
  // bissexto: 29/02 existe em 1972
  TEST_ASSERT_EQUAL_INT32(2, dias_civis(1972, 3, 1) - dias_civis(1972, 2, 28));
  TEST_ASSERT_EQUAL_INT32(365, dias_civis(1971, 1, 1));
}
void test_epoch_min_consistente() {
  int64_t meia_noite = epoch_min(2026, 6, 12, 0, 0);
  TEST_ASSERT_EQUAL_INT64(meia_noite + 8 * 60 + 30, epoch_min(2026, 6, 12, 8, 30));
  TEST_ASSERT_EQUAL_INT64(meia_noite + 24 * 60, epoch_min(2026, 6, 13, 0, 0));
}
void test_humor_limiar_de_24h() {
  int64_t agora = epoch_min(2026, 6, 12, 12, 0);
  TEST_ASSERT_EQUAL(H_FELIZ, humor(agora, agora - (24 * 60 - 1), 12));
  TEST_ASSERT_EQUAL(H_NEUTRO, humor(agora, agora - 24 * 60, 12));
}
void test_humor_limiar_de_3_dias() {
  int64_t agora = epoch_min(2026, 6, 12, 12, 0);
  TEST_ASSERT_EQUAL(H_NEUTRO, humor(agora, agora - 3 * 24 * 60, 12));
  TEST_ASSERT_EQUAL(H_CARENTE, humor(agora, agora - (3 * 24 * 60 + 1), 12));
}
void test_humor_nunca_conversou_e_neutro() {
  TEST_ASSERT_EQUAL(H_NEUTRO, humor(epoch_min(2026, 6, 12, 12, 0), 0, 12));
  // RTC regrediu (perdeu bateria) com NVS guardada: nao pode virar FELIZ
  TEST_ASSERT_EQUAL(H_NEUTRO, humor(epoch_min(2000, 1, 1, 12, 0),
                                    epoch_min(2026, 6, 12, 12, 0), 12));
}
void test_humor_janela_de_sono() {
  int64_t agora = epoch_min(2026, 6, 12, 23, 30);
  TEST_ASSERT_EQUAL(H_DORMINDO, humor(agora, agora - 10, 23));
  TEST_ASSERT_EQUAL(H_DORMINDO, humor(agora, agora - 10, 3));
  TEST_ASSERT_EQUAL(H_DORMINDO, humor(agora, agora - 10, 6));
  TEST_ASSERT_EQUAL(H_FELIZ, humor(agora, agora - 10, 7));
  TEST_ASSERT_EQUAL(H_FELIZ, humor(agora, agora - 10, 22));
}

// ---- maquina de estados ----
void test_maquina_caminho_feliz() {
  Maquina m;
  maquina_init(m, 1000);
  TEST_ASSERT_EQUAL(E_PET, m.st);
  TEST_ASSERT_TRUE(maquina_evento(m, EV_OK_APERTA, 2000));
  TEST_ASSERT_EQUAL(E_OUVINDO, m.st);
  TEST_ASSERT_TRUE(maquina_evento(m, EV_OK_SOLTA, 5000));
  TEST_ASSERT_EQUAL(E_PENSANDO, m.st);
  TEST_ASSERT_TRUE(maquina_evento(m, EV_RESPOSTA, 8000));
  TEST_ASSERT_EQUAL(E_FALANDO, m.st);
  TEST_ASSERT_TRUE(maquina_evento(m, EV_VOLTA, 9000));
  TEST_ASSERT_EQUAL(E_PET, m.st);
}
void test_maquina_falha_vai_pra_falando() {
  Maquina m;
  maquina_init(m, 0);
  maquina_evento(m, EV_OK_APERTA, 0);
  maquina_evento(m, EV_OK_SOLTA, 100);
  TEST_ASSERT_TRUE(maquina_evento(m, EV_FALHA, 200));
  TEST_ASSERT_EQUAL(E_FALANDO, m.st);
}
void test_maquina_estoura_15s_ouvindo() {
  Maquina m;
  maquina_init(m, 0);
  maquina_evento(m, EV_OK_APERTA, 1000);
  TEST_ASSERT_FALSE(maquina_tick(m, 1000 + kOuvindoMaxMs - 1));
  TEST_ASSERT_TRUE(maquina_tick(m, 1000 + kOuvindoMaxMs));
  TEST_ASSERT_EQUAL(E_PENSANDO, m.st);
}
void test_maquina_estoura_60s_falando() {
  Maquina m;
  maquina_init(m, 0);
  maquina_evento(m, EV_OK_APERTA, 0);
  maquina_evento(m, EV_OK_SOLTA, 100);
  maquina_evento(m, EV_RESPOSTA, 200);
  TEST_ASSERT_FALSE(maquina_tick(m, 200 + kFalandoMaxMs - 1));
  TEST_ASSERT_TRUE(maquina_tick(m, 200 + kFalandoMaxMs));
  TEST_ASSERT_EQUAL(E_PET, m.st);
}
void test_maquina_ignora_evento_fora_de_hora() {
  Maquina m;
  maquina_init(m, 0);
  TEST_ASSERT_FALSE(maquina_evento(m, EV_OK_SOLTA, 10));
  TEST_ASSERT_FALSE(maquina_evento(m, EV_RESPOSTA, 10));
  TEST_ASSERT_FALSE(maquina_evento(m, EV_VOLTA, 10));
  TEST_ASSERT_EQUAL(E_PET, m.st);
  TEST_ASSERT_FALSE(maquina_tick(m, 999999));  // PET nao estoura nada
}

// ---- paginacao ----
void test_linhas_quebram_no_espaco() {
  // cols=9: "um dois t" estoura -> quebra no espaco
  TEST_ASSERT_EQUAL_UINT16(3, linhas_total("um dois tres quatro", 9));
  char l[32];
  TEST_ASSERT_TRUE(linha_texto("um dois tres quatro", 0, 9, l, sizeof(l)));
  TEST_ASSERT_EQUAL_STRING("um dois", l);
  TEST_ASSERT_TRUE(linha_texto("um dois tres quatro", 1, 9, l, sizeof(l)));
  TEST_ASSERT_EQUAL_STRING("tres", l);
  TEST_ASSERT_TRUE(linha_texto("um dois tres quatro", 2, 9, l, sizeof(l)));
  TEST_ASSERT_EQUAL_STRING("quatro", l);
}
void test_acento_conta_uma_coluna() {
  // "não é" = n a~ o _ e' = 5 colunas (7 bytes)
  TEST_ASSERT_EQUAL_UINT16(1, linhas_total("n\xc3\xa3o \xc3\xa9", 5));
  char l[16];
  TEST_ASSERT_TRUE(linha_texto("n\xc3\xa3o \xc3\xa9", 0, 5, l, sizeof(l)));
  TEST_ASSERT_EQUAL_STRING("n\xc3\xa3o \xc3\xa9", l);
}
void test_palavrao_corta_seco() {
  TEST_ASSERT_EQUAL_UINT16(2, linhas_total("abcdefgh", 4));
  char l[8];
  TEST_ASSERT_TRUE(linha_texto("abcdefgh", 1, 4, l, sizeof(l)));
  TEST_ASSERT_EQUAL_STRING("efgh", l);
}
void test_quebra_de_linha_explicita() {
  TEST_ASSERT_EQUAL_UINT16(2, linhas_total("oi\ntudo", 10));
  char l[8];
  TEST_ASSERT_TRUE(linha_texto("oi\ntudo", 0, 10, l, sizeof(l)));
  TEST_ASSERT_EQUAL_STRING("oi", l);
  TEST_ASSERT_TRUE(linha_texto("oi\ntudo", 1, 10, l, sizeof(l)));
  TEST_ASSERT_EQUAL_STRING("tudo", l);
}
void test_linha_fora_do_range_e_vazio() {
  char l[8];
  TEST_ASSERT_FALSE(linha_texto("oi", 1, 10, l, sizeof(l)));
  TEST_ASSERT_EQUAL_UINT16(0, linhas_total("", 10));
  TEST_ASSERT_FALSE(linha_texto("", 0, 10, l, sizeof(l)));
}

// ---- bitspeech ----
void test_bitspeech_vogal_longa_consoante_curta() {
  Tom t;
  size_t prox;
  TEST_ASSERT_TRUE(bitspeech_next("ab", 0, t, &prox));
  TEST_ASSERT_GREATER_THAN_UINT16(600, t.freq);   // vogal: aguda
  TEST_ASSERT_EQUAL_UINT16(90, t.dur_ms);         // e longa
  TEST_ASSERT_EQUAL(1, (int)prox);
  TEST_ASSERT_TRUE(bitspeech_next("ab", 1, t, &prox));
  TEST_ASSERT_LESS_THAN_UINT16(600, t.freq);      // consoante: grave
  TEST_ASSERT_EQUAL_UINT16(45, t.dur_ms);         // e curta
  TEST_ASSERT_TRUE(bitspeech_next("z", 0, t, &prox));
  TEST_ASSERT_LESS_THAN_UINT16(600, t.freq);      // ate o z fica grave
}
void test_bitspeech_espaco_e_pontuacao_pausam() {
  Tom t;
  size_t prox;
  bitspeech_next(" ", 0, t, &prox);
  TEST_ASSERT_EQUAL_UINT16(0, t.freq);
  bitspeech_next("!", 0, t, &prox);
  TEST_ASSERT_EQUAL_UINT16(0, t.freq);
}
void test_bitspeech_acento_vira_vogal_base() {
  Tom ta, taa;
  size_t prox;
  bitspeech_next("a", 0, ta, &prox);
  bitspeech_next("\xc3\xa3", 0, taa, &prox);      // ã
  TEST_ASSERT_EQUAL_UINT16(ta.freq, taa.freq);
  TEST_ASSERT_EQUAL(2, (int)prox);                // pulou os 2 bytes
}
void test_bitspeech_deterministico_e_termina() {
  const char* s = "oi!";
  Tom a, b;
  size_t p1, p2;
  for (size_t i = 0; s[i];) {
    TEST_ASSERT_TRUE(bitspeech_next(s, i, a, &p1));
    TEST_ASSERT_TRUE(bitspeech_next(s, i, b, &p2));
    TEST_ASSERT_EQUAL_UINT16(a.freq, b.freq);
    TEST_ASSERT_EQUAL_UINT16(a.dur_ms, b.dur_ms);
    TEST_ASSERT_EQUAL(p1, p2);
    i = p1;
  }
  TEST_ASSERT_FALSE(bitspeech_next(s, 3, a, &p1));  // fim do texto
}

// ---- parse da resposta do server ----
void test_voz_parse_extrai_falei_e_resposta() {
  char falei[32], resp[64];
  TEST_ASSERT_TRUE(voz_parse(
      "{\"falei\":\"oi tudo bem\",\"resposta\":\"miau! to otimo\"}",
      falei, sizeof(falei), resp, sizeof(resp)));
  TEST_ASSERT_EQUAL_STRING("oi tudo bem", falei);
  TEST_ASSERT_EQUAL_STRING("miau! to otimo", resp);
}
void test_voz_parse_falei_opcional() {
  char resp[64];
  TEST_ASSERT_TRUE(voz_parse("{\"resposta\":\"oi\"}", nullptr, 0,
                             resp, sizeof(resp)));
  TEST_ASSERT_EQUAL_STRING("oi", resp);
}
void test_voz_parse_rejeita_sem_resposta_ou_lixo() {
  char resp[64];
  TEST_ASSERT_FALSE(voz_parse("{\"erro\":\"sem chave\"}", nullptr, 0,
                              resp, sizeof(resp)));
  TEST_ASSERT_FALSE(voz_parse("nem json", nullptr, 0, resp, sizeof(resp)));
}

// ---- registro_parse ----
void test_registro_parse_pagina_cheia() {
  const char* json =
      "{\"total\":14,\"pags\":3,\"pag\":1,\"itens\":["
      "{\"q\":\"vc gosta de bolo?\",\"r\":\"adoro! só nunca comi\"},"
      "{\"q\":\"é?\",\"r\":\"moro num nokia, né\"}]}";
  RegPar itens[6];
  uint16_t total; uint8_t pags, pag;
  uint8_t n = registro_parse(json, itens, 6, &total, &pags, &pag);
  TEST_ASSERT_EQUAL_UINT8(2, n);
  TEST_ASSERT_EQUAL_UINT16(14, total);
  TEST_ASSERT_EQUAL_UINT8(3, pags);
  TEST_ASSERT_EQUAL_UINT8(1, pag);
  TEST_ASSERT_EQUAL_STRING("vc gosta de bolo?", itens[0].q);
  TEST_ASSERT_EQUAL_STRING("adoro! só nunca comi", itens[0].r);
  TEST_ASSERT_EQUAL_STRING("moro num nokia, né", itens[1].r);
}
void test_registro_parse_vazio_e_lixo() {
  RegPar itens[6];
  uint16_t total = 99; uint8_t pags = 9, pag = 9;
  TEST_ASSERT_EQUAL_UINT8(0, registro_parse(
      "{\"total\":0,\"pags\":0,\"pag\":0,\"itens\":[]}",
      itens, 6, &total, &pags, &pag));
  TEST_ASSERT_EQUAL_UINT16(0, total);
  TEST_ASSERT_EQUAL_UINT8(0, registro_parse("{trunca", itens, 6, &total,
                                            &pags, &pag));
  TEST_ASSERT_EQUAL_UINT16(0, total);   // lixo zera, não herda valor velho
}
void test_registro_parse_respeita_max() {
  const char* json =
      "{\"total\":3,\"pags\":1,\"pag\":0,\"itens\":["
      "{\"q\":\"a\",\"r\":\"b\"},{\"q\":\"c\",\"r\":\"d\"},"
      "{\"q\":\"e\",\"r\":\"f\"}]}";
  RegPar itens[2];
  uint16_t total; uint8_t pags, pag;
  TEST_ASSERT_EQUAL_UINT8(2, registro_parse(json, itens, 2, &total,
                                            &pags, &pag));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dias_civis_ancoras);
  RUN_TEST(test_epoch_min_consistente);
  RUN_TEST(test_humor_limiar_de_24h);
  RUN_TEST(test_humor_limiar_de_3_dias);
  RUN_TEST(test_humor_nunca_conversou_e_neutro);
  RUN_TEST(test_humor_janela_de_sono);
  RUN_TEST(test_maquina_caminho_feliz);
  RUN_TEST(test_maquina_falha_vai_pra_falando);
  RUN_TEST(test_maquina_estoura_15s_ouvindo);
  RUN_TEST(test_maquina_estoura_60s_falando);
  RUN_TEST(test_maquina_ignora_evento_fora_de_hora);
  RUN_TEST(test_linhas_quebram_no_espaco);
  RUN_TEST(test_acento_conta_uma_coluna);
  RUN_TEST(test_palavrao_corta_seco);
  RUN_TEST(test_quebra_de_linha_explicita);
  RUN_TEST(test_linha_fora_do_range_e_vazio);
  RUN_TEST(test_bitspeech_vogal_longa_consoante_curta);
  RUN_TEST(test_bitspeech_espaco_e_pontuacao_pausam);
  RUN_TEST(test_bitspeech_acento_vira_vogal_base);
  RUN_TEST(test_bitspeech_deterministico_e_termina);
  RUN_TEST(test_voz_parse_extrai_falei_e_resposta);
  RUN_TEST(test_voz_parse_falei_opcional);
  RUN_TEST(test_voz_parse_rejeita_sem_resposta_ou_lixo);
  RUN_TEST(test_registro_parse_pagina_cheia);
  RUN_TEST(test_registro_parse_vazio_e_lixo);
  RUN_TEST(test_registro_parse_respeita_max);
  return UNITY_END();
}
