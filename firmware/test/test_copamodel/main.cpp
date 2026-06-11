#include <unity.h>
#include <string.h>
#include "copamodel.h"

// payload real do contrato do server (2 jogos, um com placar ao vivo)
static const char* kPayload =
    "{\"jogos\":["
    "{\"dia\":13,\"mes\":6,\"h\":19,\"m\":0,\"t1\":\"BRA\",\"t2\":\"MAR\","
    "\"info\":\"Grupo C\",\"s1\":-1,\"s2\":-1,\"live\":false},"
    "{\"dia\":11,\"mes\":6,\"h\":16,\"m\":0,\"t1\":\"MEX\",\"t2\":\"RSA\","
    "\"info\":\"Grupo A\",\"s1\":2,\"s2\":1,\"live\":true}"
    "],\"atualizado_s\":42}";

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
  RUN_TEST(test_linha_sem_placar_mostra_data);
  RUN_TEST(test_linha_com_placar_mostra_placar);
  return UNITY_END();
}
