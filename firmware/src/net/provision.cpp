#include "net/provision.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "net/credstore.h"

namespace provision {

static WebServer server_(80);
static DNSServer dns_;
static char ap_name_[20];
static char ap_pass_[9];          // 8 digitos novos a cada modo config
static uint32_t reboot_ms_ = 0;   // agendado apos salvar
static String opts_;              // cache do scan de redes
static uint32_t scan_ms_ = 0;

// pagina unica: paleta do logo da Copa (preto/dourado/branco) sobre cinza,
// wordmark espnokia em SVG pixel-art (mesmo bitmap do firmware, em RLE)
static const char kPage[] PROGMEM = R"html(<!doctype html>
<html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>espnokia · WiFi</title><style>
body{background:#b9b9b9;font-family:ui-monospace,Consolas,monospace;color:#111;
display:flex;justify-content:center;padding:24px 12px}
.card{background:#fff;border:3px solid #111;box-shadow:8px 8px 0 #111;
max-width:340px;width:100%;padding:0 0 18px}
.top{background:#111;padding:16px 14px 12px}
.top svg{display:block;width:100%;height:auto}
h1{font-size:15px;margin:14px 14px 2px;letter-spacing:1px}
p{font-size:12px;margin:4px 14px;color:#444}
.mac{color:#a07d22;font-weight:bold}
form{margin:10px 14px 0;display:grid;gap:8px}
label{font-size:11px;text-transform:uppercase;letter-spacing:1px}
input{border:2px solid #111;padding:9px 8px;font:inherit;font-size:14px;
background:#fff;border-radius:0}
input:focus{outline:2px solid #c9a13b}
button{border:2px solid #111;background:#111;color:#c9a13b;font:inherit;
font-size:14px;font-weight:bold;letter-spacing:2px;padding:11px;cursor:pointer}
button:active{background:#c9a13b;color:#111}
.foot{font-size:10px;color:#777;margin:14px 14px 0}</style></head><body>
<div class="card"><div class="top">
<svg viewBox="0 0 79 8" shape-rendering="crispEdges"><path fill="#c9a13b" d="%LOGO%"/></svg>
</div>
<h1>CONFIGURAR WIFI</h1>
<p>rede do aparelho: <b>%AP%</b></p>
<p>conectado: <span class="mac">%MAC%</span></p>
<form method="POST" action="/salvar">
<label for="s">Rede</label>
<input id="s" name="ssid" list="redes" maxlength="32" required autocomplete="off">
<datalist id="redes">%OPTS%</datalist>
<label for="p">Senha</label>
<input id="p" name="pass" type="password" maxlength="64" autocomplete="off">
<button type="submit">SALVAR</button>
</form>
<p class="foot">espnokia-phone · a senha fica cifrada no aparelho</p>
</div></body></html>)html";

static const char kSaved[] PROGMEM = R"html(<!doctype html>
<html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>espnokia · salvo</title><style>
body{background:#b9b9b9;font-family:ui-monospace,Consolas,monospace;color:#111;
display:flex;justify-content:center;padding:24px 12px}
.card{background:#fff;border:3px solid #111;box-shadow:8px 8px 0 #111;
max-width:340px;width:100%;padding:18px 14px;text-align:center}
h1{font-size:15px;letter-spacing:1px}
p{font-size:12px;color:#444}</style></head><body>
<div class="card"><h1>REDE SALVA ✓</h1>
<p>o espnokia vai reiniciar e conectar em <b>%SSID%</b></p>
<p>pode fechar esta página</p></div></body></html>)html";

// wordmark 79x8 do assets.txt em rects RLE (1 run = M{x} {y}h{w}v1h-{w}z)
static const char kLogoPath[] PROGMEM =
    "M2 0h7v1h-7zM11 0h7v1h-7zM20 0h8v1h-8zM31 0h3v1h-3zM39 0h2v1h-2zM44 0h8v1"
    "h-8zM55 0h2v1h-2zM60 0h3v1h-3zM65 0h2v1h-2zM72 0h4v1h-4zM1 1h8v1h-8zM10 1"
    "h8v1h-8zM20 1h9v1h-9zM31 1h4v1h-4zM39 1h2v1h-2zM43 1h10v1h-10zM55 1h2v1h-"
    "2zM59 1h3v1h-3zM65 1h2v1h-2zM71 1h6v1h-6zM0 2h2v1h-2zM10 2h2v1h-2zM20 2h2"
    "v1h-2zM27 2h2v1h-2zM31 2h5v1h-5zM39 2h2v1h-2zM43 2h2v1h-2zM51 2h2v1h-2zM5"
    "5 2h2v1h-2zM58 2h3v1h-3zM65 2h2v1h-2zM70 2h3v1h-3zM75 2h2v1h-2zM0 3h7v1h-"
    "7zM10 3h7v1h-7zM20 3h9v1h-9zM31 3h2v1h-2zM34 3h2v1h-2zM39 3h2v1h-2zM43 3h"
    "2v1h-2zM51 3h2v1h-2zM55 3h5v1h-5zM65 3h2v1h-2zM69 3h4v1h-4zM75 3h3v1h-3zM"
    "0 4h7v1h-7zM11 4h7v1h-7zM20 4h8v1h-8zM31 4h2v1h-2zM35 4h2v1h-2zM39 4h2v1h"
    "-2zM43 4h2v1h-2zM51 4h2v1h-2zM55 4h5v1h-5zM65 4h2v1h-2zM69 4h2v1h-2zM75 4"
    "h2v1h-2zM0 5h2v1h-2zM17 5h2v1h-2zM20 5h2v1h-2zM31 5h2v1h-2zM36 5h2v1h-2zM"
    "39 5h2v1h-2zM43 5h2v1h-2zM51 5h2v1h-2zM55 5h2v1h-2zM58 5h3v1h-3zM65 5h2v1"
    "h-2zM68 5h10v1h-10zM1 6h8v1h-8zM10 6h8v1h-8zM20 6h2v1h-2zM31 6h2v1h-2zM37"
    " 6h4v1h-4zM43 6h10v1h-10zM55 6h2v1h-2zM59 6h3v1h-3zM65 6h2v1h-2zM68 6h10v"
    "1h-10zM2 7h7v1h-7zM10 7h7v1h-7zM20 7h2v1h-2zM31 7h2v1h-2zM38 7h3v1h-3zM44"
    " 7h8v1h-8zM55 7h2v1h-2zM60 7h3v1h-3zM65 7h2v1h-2zM68 7h2v1h-2zM76 7h2v1h-2z";

const char* ap_name() { return ap_name_; }
const char* ap_pass() { return ap_pass_; }

// so o final do MAC de quem conectou no AP, pro usuario se reconhecer
static String cliente_mac() {
  wifi_sta_list_t lst;
  if (esp_wifi_ap_get_sta_list(&lst) == ESP_OK && lst.num > 0) {
    const uint8_t* m = lst.sta[0].mac;
    char buf[12];
    snprintf(buf, sizeof(buf), "...%02X:%02X", m[4], m[5]);
    return String(buf);
  }
  return String("ninguem ainda");
}

static String scan_opts() {
  uint32_t now = millis();
  if (opts_.length() && now - scan_ms_ < 15000) return opts_;  // cache 15 s
  scan_ms_ = now;
  opts_ = "";
  int n = WiFi.scanNetworks();  // AP_STA permite escanear com o AP de pe
  for (int i = 0; i < n && i < 12; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    s.replace("\"", "&quot;");
    opts_ += "<option value=\"" + s + "\">";
  }
  WiFi.scanDelete();
  return opts_;
}

static void raiz() {
  String html = FPSTR(kPage);
  html.replace("%LOGO%", FPSTR(kLogoPath));
  html.replace("%AP%", ap_name_);
  html.replace("%MAC%", cliente_mac());
  html.replace("%OPTS%", scan_opts());
  server_.send(200, "text/html", html);
}

static void salvar() {
  String ssid = server_.arg("ssid");
  String pass = server_.arg("pass");
  if (!ssid.length()) {
    server_.sendHeader("Location", "/");
    server_.send(302, "text/plain", "");
    return;
  }
  credstore::save(ssid.c_str(), pass.c_str());
  String html = FPSTR(kSaved);
  html.replace("%SSID%", ssid);
  server_.send(200, "text/html", html);
  reboot_ms_ = millis() + 2500;  // tempo da resposta sair antes do restart
  Serial.printf("[prov] rede '%s' salva, reiniciando\n", ssid.c_str());
}

static void captive() {  // qualquer URL desconhecida cai na pagina
  server_.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
  server_.send(302, "text/plain", "");
}

void start() {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(ap_name_, sizeof(ap_name_), "espnokia-%04X", (uint16_t)(mac >> 32));
  // senha WPA2 de 8 digitos sorteada agora (HW RNG): o dono le na tela do
  // aparelho (Config > Wifi); vizinho nao entra no AP nem ve a pagina
  for (uint8_t i = 0; i < 8; i++) ap_pass_[i] = '0' + (esp_random() % 10);
  ap_pass_[8] = '\0';
  WiFi.mode(WIFI_AP_STA);  // STA junto pro scan de redes funcionar
  WiFi.softAP(ap_name_, ap_pass_, 1, 0, 1);  // WPA2, canal 1, SO 1 cliente
  dns_.start(53, "*", WiFi.softAPIP());      // captive: todo DNS aponta pra ca
  server_.on("/", raiz);
  server_.on("/salvar", HTTP_POST, salvar);
  server_.onNotFound(captive);
  server_.begin();
  Serial.printf("[prov] modo config: AP %s senha %s em %s\n", ap_name_,
                ap_pass_, WiFi.softAPIP().toString().c_str());
}

void tick(uint32_t) {
  dns_.processNextRequest();
  server_.handleClient();
  if (reboot_ms_ && millis() > reboot_ms_) ESP.restart();
}

}  // namespace provision
