#include "voicecli.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if __has_include("espnokia_config.h")
#include "espnokia_config.h"
#else
#define SERVER_URL ""
#define DEVICE_KEY ""
#endif

namespace voicecli {

static const uint32_t kRespTimeoutMs = 20000;

static WiFiClient plain_;
static WiFiClientSecure tls_;
static WiFiClient* cli_ = nullptr;
static size_t enviados_ = 0;  // bytes de audio ja subidos (diagnostico)

// SERVER_URL = "http[s]://host[:porta]" (sem barra no fim)
static bool parse_url(bool& https, char* host, size_t host_len,
                      uint16_t& porta) {
  const char* u = SERVER_URL;
  https = strncmp(u, "https://", 8) == 0;
  if (!https && strncmp(u, "http://", 7) != 0) return false;
  u += https ? 8 : 7;
  const char* dois = strchr(u, ':');
  size_t hlen = dois ? (size_t)(dois - u) : strlen(u);
  if (hlen == 0 || hlen >= host_len) return false;
  memcpy(host, u, hlen);
  host[hlen] = '\0';
  porta = dois ? (uint16_t)atoi(dois + 1) : (https ? 443 : 80);
  return true;
}

bool conecta() {
  if (cli_ && cli_->connected()) return true;  // socket quente: nada a fazer
  abort();
  bool https;
  char host[64];
  uint16_t porta;
  if (!parse_url(https, host, sizeof(host), porta)) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (https) tls_.setInsecure();  // o segredo do device e a chave, nao o canal
  cli_ = https ? (WiFiClient*)&tls_ : &plain_;
  uint32_t t0 = millis();
  if (!cli_->connect(host, porta, 5000)) {
    Serial.printf("[voz] connect falhou (heap %u)\n", ESP.getFreeHeap());
    cli_ = nullptr;
    return false;
  }
  Serial.printf("[voz] conectado em %u ms (heap %u)\n",
                (unsigned)(millis() - t0), ESP.getFreeHeap());
  return true;
}

bool begin(const char* path) {
  if (!conecta()) return false;  // ja vivo na pratica: conecta() no on_enter
  bool https;
  char host[64];
  uint16_t porta;
  if (!parse_url(https, host, sizeof(host), porta)) return false;
  enviados_ = 0;
  char cab[384];
  snprintf(cab, sizeof(cab),
           "POST %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "X-Device-Key: " DEVICE_KEY "\r\n"
           "Content-Type: application/octet-stream\r\n"
           "Transfer-Encoding: chunked\r\n"
           "Connection: keep-alive\r\n\r\n",
           path, host);
  cli_->print(cab);
  return true;
}

bool write(const uint8_t* dados, size_t n) {
  if (!cli_) return false;
  if (n == 0) return true;
  char tam[12];
  snprintf(tam, sizeof(tam), "%X\r\n", (unsigned)n);
  cli_->print(tam);
  size_t mandou = cli_->write(dados, n);
  if (mandou != n || !cli_->connected()) {
    Serial.printf("[voz] tls caiu: write %u/%u conn=%d apos %u bytes\n",
                  (unsigned)mandou, (unsigned)n, cli_->connected(),
                  (unsigned)enviados_);
    abort();
    return false;
  }
  cli_->print("\r\n");
  enviados_ += n;
  return true;
}

// le status + headers + corpo e devolve o status HTTP (<0 = rede); se o
// corpo veio inteiro delimitado e o socket segue de pe, guarda a conexao
static int le_resposta(char* resp, size_t resp_len, uint32_t timeout_ms) {
  uint32_t t0 = millis();
  while (!cli_->available()) {
    if (millis() - t0 > timeout_ms || !cli_->connected()) {
      Serial.printf("[voz] sem resposta (%s apos %u ms)\n",
                    cli_->connected() ? "timeout" : "conexao caiu",
                    (unsigned)(millis() - t0));
      abort();
      return -2;
    }
    delay(10);
  }
  cli_->setTimeout(timeout_ms);  // readBytesUntil; default de 1 s racharia o parse
  // status line
  char linha[96];
  size_t n = cli_->readBytesUntil('\n', linha, sizeof(linha) - 1);
  linha[n] = '\0';
  int status = 0;
  if (sscanf(linha, "HTTP/%*s %d", &status) != 1) {
    abort();
    return -3;
  }
  // headers: so interessa o tamanho do corpo
  long content_len = -1;
  while (true) {
    n = cli_->readBytesUntil('\n', linha, sizeof(linha) - 1);
    linha[n] = '\0';
    if (n == 0 || linha[0] == '\r') break;
    if (strncasecmp(linha, "Content-Length:", 15) == 0)
      content_len = atol(linha + 15);
  }
  // corpo (JSON pequeno): le ate o content-length, a conexao fechar ou encher
  size_t i = 0;
  t0 = millis();
  while (i < resp_len - 1 && millis() - t0 < timeout_ms) {
    if (content_len >= 0 && (long)i >= content_len) break;
    if (cli_->available()) {
      resp[i++] = (char)cli_->read();
      t0 = millis();
    } else if (!cli_->connected()) {
      break;
    } else {
      delay(5);
    }
  }
  resp[i] = '\0';
  Serial.printf("[voz] resposta status=%d (%u bytes)\n", status, (unsigned)i);
  // corpo delimitado e socket de pe: guarda a conexao pra proxima fala
  if (content_len < 0 || (long)i != content_len || !cli_->connected()) abort();
  return status;
}

static uint32_t espera_t0_ = 0;  // inicio da espera do finish_poll

bool finish_begin() {
  if (!cli_) return false;
  size_t fim_ok = cli_->print("0\r\n\r\n");  // fim do corpo chunked
  Serial.printf("[voz] finish: %u bytes subidos, fim=%u conn=%d\n",
                (unsigned)enviados_, (unsigned)fim_ok, cli_->connected());
  espera_t0_ = millis();
  return fim_ok > 0;
}

int finish_poll(char* resp, size_t resp_len) {
  if (!cli_ || resp_len == 0) return -1;
  if (!cli_->available()) {  // nada ainda: devolve o loop pra tela animar
    if (millis() - espera_t0_ <= kRespTimeoutMs && cli_->connected())
      return kEsperando;
    Serial.printf("[voz] sem resposta (%s apos %u ms)\n",
                  cli_->connected() ? "timeout" : "conexao caiu",
                  (unsigned)(millis() - espera_t0_));
    abort();
    return -2;
  }
  // o status chegou; headers + corpo vem juntos (JSON pequeno) em ms
  return le_resposta(resp, resp_len, kRespTimeoutMs);
}

bool ping() {
  if (!conecta()) return false;  // caiu: reconecta agora, fora do aperto
  bool https;
  char host[64];
  uint16_t porta;
  if (!parse_url(https, host, sizeof(host), porta)) return false;
  char cab[128];
  snprintf(cab, sizeof(cab),
           "GET /health HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Connection: keep-alive\r\n\r\n",
           host);
  if (cli_->print(cab) == 0) {
    abort();
    return false;
  }
  char resp[64];  // {"status":"ok"} — so consome pra manter o socket sao
  return le_resposta(resp, sizeof(resp), 3000) == 200;
}

void abort() {
  if (cli_) {
    cli_->stop();
    cli_ = nullptr;
  }
}

}  // namespace voicecli
