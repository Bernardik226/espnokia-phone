#include "net/wifi.h"
#include <Arduino.h>
#include <WiFi.h>
// "config.h" puro colide com headers do SDK do ESP32 — dai o prefixo
#if __has_include("espnokia_config.h")
#include "espnokia_config.h"
#else
#include "espnokia_config.example.h"  // builda sem segredos; copie na bancada
#endif

namespace wifi {

static uint32_t last_try_ = 0;
static bool was_connected_ = false;

void init() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // backoff manual no tick
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

bool connected() { return WiFi.status() == WL_CONNECTED; }

uint8_t level() {
  if (!connected()) return 0;
  int rssi = WiFi.RSSI();
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

void tick(uint32_t now_ms) {
  bool up = connected();
  if (up && !was_connected_)
    Serial.printf("[wifi] conectado %s rssi=%d\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  if (!up && was_connected_) Serial.println("[wifi] caiu, tentando de novo");
  was_connected_ = up;
  if (up) return;
  if (now_ms - last_try_ < 10000) return;  // backoff de 10 s
  last_try_ = now_ms;
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

}  // namespace wifi
