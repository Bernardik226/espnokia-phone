#include "net/credstore.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include "mbedtls/sha256.h"

namespace credstore {

static Preferences prefs_;
static bool open_ = false;
static void ensure() {
  if (open_) return;
  prefs_.begin("espnokia");
  open_ = true;
}

// keystream estilo CTR: bloco i = SHA256(chave || i); chave = SHA256 do
// eFuse MAC + salt. XOR e simetrico, a mesma funcao cifra e decifra.
static void crypt(uint8_t* data, size_t len) {
  uint8_t seed[16] = {'e', 's', 'p', 'n', 'o', 'k', 'i', 'a'};
  uint64_t mac = ESP.getEfuseMac();
  memcpy(seed + 8, &mac, 8);
  uint8_t key[32];
  mbedtls_sha256(seed, sizeof(seed), key, 0);
  uint8_t block[36], ks[32];
  for (size_t i = 0; i < len; i += 32) {
    uint32_t ctr = (uint32_t)(i / 32);
    memcpy(block, key, 32);
    memcpy(block + 32, &ctr, 4);
    mbedtls_sha256(block, sizeof(block), ks, 0);
    for (size_t j = 0; j < 32 && i + j < len; j++) data[i + j] ^= ks[j];
  }
}

static bool load_blob(const char* k, char* out, size_t out_len) {
  size_t n = prefs_.getBytesLength(k);
  if (n == 0 || n >= out_len) return false;
  prefs_.getBytes(k, out, n);
  crypt((uint8_t*)out, n);
  out[n] = '\0';
  return true;
}

static void save_blob(const char* k, const char* v) {
  uint8_t buf[96];
  size_t n = strlen(v);
  if (n > sizeof(buf)) n = sizeof(buf);
  memcpy(buf, v, n);
  crypt(buf, n);
  prefs_.putBytes(k, buf, n);
}

bool present() {
  ensure();
  return prefs_.getBytesLength("wf_s") > 0;
}

bool load(char* ssid, size_t ssid_len, char* pass, size_t pass_len) {
  ensure();
  if (!load_blob("wf_s", ssid, ssid_len)) return false;
  if (!load_blob("wf_p", pass, pass_len)) pass[0] = '\0';  // rede aberta
  return true;
}

void save(const char* ssid, const char* pass) {
  ensure();
  save_blob("wf_s", ssid);
  save_blob("wf_p", pass);
}

void forget() {
  ensure();
  prefs_.remove("wf_s");
  prefs_.remove("wf_p");
}

bool take_force_ap() {
  ensure();
  bool v = prefs_.getBool("wf_ap", false);
  if (v) prefs_.putBool("wf_ap", false);
  return v;
}

void set_force_ap() {
  ensure();
  prefs_.putBool("wf_ap", true);
}

}  // namespace credstore
