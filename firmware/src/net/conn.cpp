#include "net/conn.h"

#include <Preferences.h>
#include <esp_system.h>  // esp_random
#include <stdio.h>
#include <string.h>

#if __has_include("espnokia_config.h")
#include "espnokia_config.h"
#else
#include "espnokia_config.example.h"
#endif
#ifndef SERVER_URL
#define SERVER_URL ""
#endif
#ifndef DEVICE_KEY
#define DEVICE_KEY ""
#endif

namespace conn {

static Preferences prefs_;
static char key_[40];
static char url_[96];

// 16 bytes do gerador de hardware -> 32 chars hex (128 bits de entropia)
static void gera_chave(char* out) {
  for (int i = 0; i < 16; i++)
    sprintf(out + i * 2, "%02x", (unsigned)(esp_random() & 0xFF));
  out[32] = '\0';
}

void init() {
  prefs_.begin("conn", false);
  if (prefs_.isKey("key")) prefs_.getString("key", key_, sizeof(key_));
  if (strlen(key_) < 16) {           // sem chave na NVS ainda
    if (strlen(DEVICE_KEY) >= 16)    // semeia da config.h (dev/retrocompat)...
      snprintf(key_, sizeof(key_), "%s", DEVICE_KEY);
    else                             // ...ou sorteia uma e guarda pra sempre
      gera_chave(key_);
    prefs_.putString("key", key_);
  }
  if (prefs_.isKey("url")) prefs_.getString("url", url_, sizeof(url_));
  if (!url_[0]) snprintf(url_, sizeof(url_), "%s", SERVER_URL);  // fallback
}

const char* server_url() { return url_; }
const char* device_key() { return key_; }

void set_server_url(const char* u) {
  snprintf(url_, sizeof(url_), "%s", u ? u : "");
  prefs_.putString("url", url_);
}

const char* pair_link(char* out, size_t len) {
  snprintf(out, len, "%s/#k=%s", url_, key_);
  return out;
}

}  // namespace conn
