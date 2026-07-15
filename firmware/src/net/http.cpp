#include "net/http.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "net/conn.h"
#include "net/wifi.h"

namespace http {

int get_json(const char* path, char* buf, size_t len) {
  if (!wifi::connected()) return -1;
  HTTPClient cli;
  cli.setTimeout(5000);
  String url = String(conn::server_url()) + path;
  // server em host com TLS tambem funciona: sem validar a CA (o ESP32 nao
  // tem bundle atualizado) — o segredo do device e a chave, nao o canal
  WiFiClient plain;
  WiFiClientSecure tls;
  bool https = url.startsWith("https://");
  if (https) tls.setInsecure();
  if (!cli.begin(https ? (WiFiClient&)tls : plain, url)) return -2;
  cli.addHeader(conn::kHeaderDeviceKey, conn::device_key());
  int code = cli.GET();
  if (code == HTTP_CODE_OK) {
    String body = cli.getString();
    snprintf(buf, len, "%s", body.c_str());
  }
  cli.end();
  return code;
}

}  // namespace http
