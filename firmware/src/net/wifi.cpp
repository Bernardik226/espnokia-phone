#include "net/wifi.h"
#include <Arduino.h>
#include <WiFi.h>
#include "net/credstore.h"
#include "net/provision.h"

namespace wifi {

static char ssid_[33];
static char pass_[65];
static bool prov_ = false;
static uint32_t last_try_ = 0;
static bool was_connected_ = false;

void init() {
  bool force_ap = credstore::take_force_ap();  // "trocar rede" do menu
  if (force_ap || !credstore::load(ssid_, sizeof(ssid_), pass_, sizeof(pass_))) {
    prov_ = true;
    provision::start();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // modem sleep custa ate segundos por requisicao
  WiFi.setAutoReconnect(false);  // backoff manual no tick
  WiFi.begin(ssid_, pass_);
}

bool connected() { return !prov_ && WiFi.status() == WL_CONNECTED; }

bool provisioning() { return prov_; }

const char* ssid() { return prov_ ? provision::ap_name() : ssid_; }

const char* ap_pass() { return prov_ ? provision::ap_pass() : ""; }

void ip_str(char* buf, size_t len) {
  IPAddress ip = prov_ ? WiFi.softAPIP() : WiFi.localIP();
  snprintf(buf, len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

uint8_t level() {
  if (!connected()) return 0;
  int rssi = WiFi.RSSI();
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

void forget() {
  credstore::forget();
  delay(100);
  ESP.restart();  // sobe direto no modo config (sem rede salva)
}

void reconfigure() {
  credstore::set_force_ap();
  delay(100);
  ESP.restart();  // modo config; a rede antiga fica ate o POST sobrescrever
}

void tick(uint32_t now_ms) {
  if (prov_) {
    provision::tick(now_ms);
    return;
  }
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
  WiFi.begin(ssid_, pass_);
}

}  // namespace wifi
