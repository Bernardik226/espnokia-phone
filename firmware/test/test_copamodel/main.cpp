#include <unity.h>
#include <string.h>
#include "copamodel.h"

// payload real do contrato do server (2 jogos, um com placar ao vivo)
static const char* kPayload =
    "{\"jogos\":["
    "{\"dia\":13,\"mes\":6,\"h\":19,\"m\":0,\"t1\":\"BRA\",\"t2\":\"MAR\","
    "\"info\":\"Grupo C\",\"s1\":-1,\"s2\":-1,\"live\":false},"
    "{\"dia\":11,\"mes\":6,\"h\":16,\"m\":0,\"t1\":\"MEX\",\"t2\":\"RSA\","
    "\"info\":\"Grupo A\",\"s1\":2,\"s2\":1,\"live\":true,\"min\":\"67\","
    "\"est\":\"Estadio Azteca · Mexico City\","
    "\"g1\":\"J. Quinones 9'\\nR. Jimenez 67'\"}"
    "],\"atualizado_s\":42}";

// payload de /futebol/jogos: clube traz nome completo em n1/n2
static const char* kFutebol =
    "{\"jogos\":["
    "{\"dia\":12,\"mes\":6,\"h\":17,\"m\":0,\"t1\":\"FLA\",\"t2\":\"VAS\","
    "\"info\":\"Brasileirão\",\"s1\":2,\"s2\":1,\"live\":true,"
    "\"n1\":\"Flamengo\",\"n2\":\"Vasco\"}"
    "],\"atualizado_s\":0}";

static const char* kLigas =
    "{\"ligas\":["
    "{\"id\":\"bra.1\",\"n\":\"Brasileirão\"},"
    "{\"id\":\"uefa.champions\",\"n\":\"Champions\"},"
    "{\"n\":\"Sem id\"}"
    "]}";

static const char* kGrupos =
    "{\"grupos\":["
    "{\"n\":\"A\",\"t\":["
    "{\"c\":\"MEX\",\"p\":3,\"j\":1,\"s\":2},"
    "{\"c\":\"KOR\",\"p\":0,\"j\":0,\"s\":0},"
    "{\"c\":\"CZE\",\"p\":0,\"j\":0,\"s\":0},"
    "{\"c\":\"RSA\",\"p\":0,\"j\":1,\"s\":-2}]},"
    "{\"n\":\"B\",\"t\":[{\"c\":\"CAN\",\"p\":0,\"j\":0,\"s\":0}]}"
    "]}";

void test_parse_payload_normal() {
  CopaJogo jogos[8];
  uint32_t idade = 0;
  uint8_t n = copa_parse(kPayload, jogos, 8, &idade);
  TEST_ASSERT_EQUAL_UINT8(2, n);
  TEST_ASSERT_EQUAL_UINT32(42, idade);
  TEST_ASSERT_EQUAL_UINT8(13, jogos[0].dia);
  TEST_ASSERT_EQUAL_UINT8(19, jogos[0].h);
  TEST_ASSERT_EQUAL_STRING("BRA", jogos[0].t1);
  TEST_ASSERT_EQUAL_STRING("MAR", jogos[0].t2);
  TEST_ASSERT_EQUAL_STRING("Grupo C", jogos[0].info);
  TEST_ASSERT_FALSE(jogos[0].live);
  TEST_ASSERT_TRUE(jogos[1].live);
}

void test_placar_ausente_fica_menos_um() {
  CopaJogo jogos[8];
  copa_parse(kPayload, jogos, 8, nullptr);
  TEST_ASSERT_EQUAL_INT8(-1, jogos[0].s1);
  TEST_ASSERT_EQUAL_INT8(2, jogos[1].s1);
  TEST_ASSERT_EQUAL_INT8(1, jogos[1].s2);
}

void test_trunca_em_max() {
  CopaJogo jogos[1];
  uint8_t n = copa_parse(kPayload, jogos, 1, nullptr);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_STRING("BRA", jogos[0].t1);
}

void test_json_invalido_da_zero() {
  CopaJogo jogos[4];
  TEST_ASSERT_EQUAL_UINT8(0, copa_parse("nao e json {", jogos, 4, nullptr));
  TEST_ASSERT_EQUAL_UINT8(0, copa_parse("{\"sem_jogos\":1}", jogos, 4, nullptr));
}

void test_minuto_gols_e_estadio_do_jogo_live() {
  CopaJogo jogos[8];
  copa_parse(kPayload, jogos, 8, nullptr);
  TEST_ASSERT_EQUAL_STRING("67", jogos[1].min);
  TEST_ASSERT_EQUAL_STRING("Estadio Azteca · Mexico City", jogos[1].est);
  TEST_ASSERT_EQUAL_STRING("J. Quinones 9'\nR. Jimenez 67'", jogos[1].g1);
  TEST_ASSERT_EQUAL_STRING("", jogos[1].g2);
}

void test_campos_extras_ausentes_ficam_vazios() {
  CopaJogo jogos[8];
  copa_parse(kPayload, jogos, 8, nullptr);
  TEST_ASSERT_EQUAL_STRING("", jogos[0].min);
  TEST_ASSERT_EQUAL_STRING("", jogos[0].est);
  TEST_ASSERT_EQUAL_STRING("", jogos[0].g1);
}

void test_nome_completo_do_clube_em_n1_n2() {
  CopaJogo jogos[8];
  TEST_ASSERT_EQUAL_UINT8(1, copa_parse(kFutebol, jogos, 8, nullptr));
  TEST_ASSERT_EQUAL_STRING("Flamengo", jogos[0].n1);
  TEST_ASSERT_EQUAL_STRING("Vasco", jogos[0].n2);
  TEST_ASSERT_EQUAL_STRING("Brasileirão", jogos[0].info);
}

void test_copa_sem_n1_n2_fica_vazio() {
  CopaJogo jogos[8];
  copa_parse(kPayload, jogos, 8, nullptr);
  TEST_ASSERT_EQUAL_STRING("", jogos[0].n1);
  TEST_ASSERT_EQUAL_STRING("", jogos[0].n2);
}

void test_parse_ligas() {
  FutLiga ligas[8];
  uint8_t n = fut_parse_ligas(kLigas, ligas, 8);
  TEST_ASSERT_EQUAL_UINT8(2, n);  // entrada sem id e descartada
  TEST_ASSERT_EQUAL_STRING("bra.1", ligas[0].id);
  TEST_ASSERT_EQUAL_STRING("Brasileirão", ligas[0].n);
  TEST_ASSERT_EQUAL_STRING("uefa.champions", ligas[1].id);
}

void test_parse_ligas_json_invalido_da_zero() {
  FutLiga ligas[8];
  TEST_ASSERT_EQUAL_UINT8(0, fut_parse_ligas("xx", ligas, 8));
  TEST_ASSERT_EQUAL_UINT8(0, fut_parse_ligas("{\"nada\":1}", ligas, 8));
}

void test_parse_grupos() {
  CopaGrupo gs[12];
  uint8_t n = copa_parse_grupos(kGrupos, gs, 12);
  TEST_ASSERT_EQUAL_UINT8(2, n);
  TEST_ASSERT_EQUAL_STRING("A", gs[0].nome);
  TEST_ASSERT_EQUAL_UINT8(4, gs[0].nt);
  TEST_ASSERT_EQUAL_STRING("MEX", gs[0].t[0].c);
  TEST_ASSERT_EQUAL_INT8(3, gs[0].t[0].pts);
  TEST_ASSERT_EQUAL_INT8(1, gs[0].t[0].j);
  TEST_ASSERT_EQUAL_INT8(2, gs[0].t[0].sg);
  TEST_ASSERT_EQUAL_INT8(-2, gs[0].t[3].sg);
  TEST_ASSERT_EQUAL_UINT8(1, gs[1].nt);
}

void test_parse_grupos_json_invalido_da_zero() {
  CopaGrupo gs[12];
  TEST_ASSERT_EQUAL_UINT8(0, copa_parse_grupos("xx", gs, 12));
  TEST_ASSERT_EQUAL_UINT8(0, copa_parse_grupos("{\"nada\":1}", gs, 12));
}

void test_ultimo_gol_pega_a_ultima_linha() {
  TEST_ASSERT_EQUAL_STRING("R. Jimenez 67'",
                           copa_ultimo_gol("J. Quinones 9'\nR. Jimenez 67'"));
  TEST_ASSERT_EQUAL_STRING("X 12'", copa_ultimo_gol("X 12'"));
  TEST_ASSERT_EQUAL_STRING("", copa_ultimo_gol(""));
}

void test_linha_sem_placar_mostra_data() {
  CopaJogo jogos[8];
  copa_parse(kPayload, jogos, 8, nullptr);
  char out[24];
  copa_linha(jogos[0], out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("13/6 BRA x MAR", out);
}

void test_linha_com_placar_mostra_placar() {
  CopaJogo jogos[8];
  copa_parse(kPayload, jogos, 8, nullptr);
  char out[24];
  copa_linha(jogos[1], out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("MEX 2x1 RSA", out);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_payload_normal);
  RUN_TEST(test_placar_ausente_fica_menos_um);
  RUN_TEST(test_trunca_em_max);
  RUN_TEST(test_json_invalido_da_zero);
  RUN_TEST(test_minuto_gols_e_estadio_do_jogo_live);
  RUN_TEST(test_campos_extras_ausentes_ficam_vazios);
  RUN_TEST(test_nome_completo_do_clube_em_n1_n2);
  RUN_TEST(test_copa_sem_n1_n2_fica_vazio);
  RUN_TEST(test_parse_ligas);
  RUN_TEST(test_parse_ligas_json_invalido_da_zero);
  RUN_TEST(test_parse_grupos);
  RUN_TEST(test_parse_grupos_json_invalido_da_zero);
  RUN_TEST(test_ultimo_gol_pega_a_ultima_linha);
  RUN_TEST(test_linha_sem_placar_mostra_data);
  RUN_TEST(test_linha_com_placar_mostra_placar);
  return UNITY_END();
}
