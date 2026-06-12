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
static String json_;              // cache do scan de redes (JSON)
static uint32_t scan_ms_ = 0;

// pagina unica: paleta do logotipo espnokia (azul #1d4487 / laranja #e56825)
// sobre o cinza, wordmark bicolor em SVG pixel-art (mesmo bitmap do firmware,
// em RLE). A lista de redes vem do /scan via JS: cadeado + barras de sinal,
// toque preenche o campo; rede oculta entra digitando o nome.
static const char kPage[] PROGMEM = R"html(<!doctype html>
<html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>espnokia · WiFi</title><style>
body{background:#b9b9b9;font-family:ui-monospace,Consolas,monospace;
color:#1d4487;display:flex;justify-content:center;padding:24px 12px}
.card{background:#fff;border:3px solid #1d4487;box-shadow:8px 8px 0 #1d4487;
max-width:340px;width:100%;padding:0 0 18px}
.top{padding:16px 14px 12px;border-bottom:3px solid #e56825}
.top svg{display:block;width:100%;height:auto}
h1{font-size:15px;margin:14px 14px 2px;letter-spacing:1px}
p{font-size:12px;margin:4px 14px;color:#44598f}
.mac{color:#e56825;font-weight:bold}
form{margin:10px 14px 0;display:grid;gap:8px}
label{font-size:11px;text-transform:uppercase;letter-spacing:1px}
input{border:2px solid #1d4487;padding:9px 8px;font:inherit;font-size:14px;
background:#fff;border-radius:0;color:#1d4487}
input:focus{outline:2px solid #e56825}
button{border:2px solid #1d4487;background:#1d4487;color:#fff;font:inherit;
font-size:14px;font-weight:bold;letter-spacing:2px;padding:11px;cursor:pointer}
button:active{background:#e56825;border-color:#e56825}
.ghost{background:#fff;color:#1d4487;font-size:12px;letter-spacing:1px;padding:8px}
.ghost:active{color:#fff}
#lst{border:2px solid #1d4487;max-height:222px;overflow-y:auto}
.net{display:flex;align-items:center;gap:8px;padding:9px 8px;cursor:pointer;
font-size:14px;border-bottom:1px solid #dde3f0}
.net:last-child{border-bottom:none}
.net.sel{background:#1d4487;color:#fff}
.net .nm{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.bars{display:flex;gap:2px;align-items:flex-end;height:13px}
.bars i{width:4px;background:#c9d2e6}
.bars i.on{background:#e56825}
.hint{font-size:10px;color:#777;margin:0}
.foot{font-size:10px;color:#777;margin:14px 14px 0}</style></head><body>
<div class="card"><div class="top">
<svg viewBox="0 0 79 8" shape-rendering="crispEdges"><path fill="#e56825" d="%LE%"/><path fill="#1d4487" d="%LN%"/></svg>
</div>
<h1>CONFIGURAR WIFI</h1>
<p>rede do aparelho: <b>%AP%</b></p>
<p>conectado: <span class="mac">%MAC%</span></p>
<form method="POST" action="/salvar">
<label>Redes próximas</label>
<div id="lst"><div class="net"><span class="nm">buscando redes...</span></div></div>
<button type="button" class="ghost" id="re">&#8635; BUSCAR REDES PRÓXIMAS</button>
<label for="s">Rede</label>
<input id="s" name="ssid" maxlength="32" required autocomplete="off"
 placeholder="toque na lista ou digite">
<p class="hint">rede oculta? digite o nome dela acima</p>
<label for="p">Senha</label>
<input id="p" name="pass" type="password" maxlength="64" autocomplete="off">
<button type="submit">SALVAR</button>
</form>
<p class="foot">espnokia-phone · a senha fica cifrada no aparelho</p>
</div>
<script>
var L=document.getElementById('lst'),S=document.getElementById('s'),
R=document.getElementById('re');
var LOCK='<svg width="10" height="11" viewBox="0 0 7 8" shape-rendering="crispEdges"><path fill="currentColor" d="M2 0h3v1h-3zM1 1h1v2h-1zM5 1h1v2h-1zM0 3h7v2h-7zM0 5h3v1h-3zM4 5h3v1h-3zM0 6h7v2h-7z"/></svg>';
function row(t){return '<div class="net"><span class="nm">'+t+'</span></div>'}
function render(ns){
 if(!ns.length){L.innerHTML=row('nenhuma rede encontrada');return}
 L.innerHTML='';
 ns.forEach(function(n){
  var d=document.createElement('div');d.className='net';
  var k=n.r>=-55?4:n.r>=-65?3:n.r>=-75?2:1,b='<span class="bars">';
  for(var i=1;i<=4;i++)b+='<i'+(i<=k?' class="on"':'')+' style="height:'+(i*3+1)+'px"></i>';
  b+='</span>';
  d.innerHTML='<span class="nm"></span>'+(n.l?LOCK:'')+b;
  d.firstChild.textContent=n.s;
  d.onclick=function(){S.value=n.s;
   for(var j=0;j<L.children.length;j++)L.children[j].className='net';
   d.className='net sel';document.getElementById('p').focus()};
  L.appendChild(d)})}
function scan(f){
 R.disabled=true;L.innerHTML=row('buscando redes...');
 fetch('/scan'+(f?'?f=1':'')).then(function(r){return r.json()}).then(render)
 .catch(function(){L.innerHTML=row('falha na busca — tente de novo')})
 .then(function(){R.disabled=false})}
R.onclick=function(){scan(1)};
scan(0);
</script>
</body></html>)html";

static const char kSaved[] PROGMEM = R"html(<!doctype html>
<html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>espnokia · salvo</title><style>
body{background:#b9b9b9;font-family:ui-monospace,Consolas,monospace;
color:#1d4487;display:flex;justify-content:center;padding:24px 12px}
.card{background:#fff;border:3px solid #1d4487;box-shadow:8px 8px 0 #1d4487;
max-width:340px;width:100%;padding:18px 14px;text-align:center}
h1{font-size:15px;letter-spacing:1px}
.ok{color:#e56825}
p{font-size:12px;color:#44598f}</style></head><body>
<div class="card"><h1>REDE SALVA <span class="ok">✓</span></h1>
<p>o espnokia vai reiniciar e conectar em <b>%SSID%</b></p>
<p>pode fechar esta página</p></div></body></html>)html";

// wordmark 79x8 do assets.txt em rects RLE (1 run = M{x} {y}h{w}v1h-{w}z),
// dividido em dois paths pra pintar bicolor: ESP (x<31) laranja, NOKIA azul
static const char kLogoEsp[] PROGMEM =
    "M2 0h7v1h-7zM11 0h7v1h-7zM20 0h8v1h-8zM1 1h8v1h-8zM10 1h8v1h-8zM20 1h9v1h-9z"
    "M0 2h2v1h-2zM10 2h2v1h-2zM20 2h2v1h-2zM27 2h2v1h-2zM0 3h7v1h-7zM10 3h7v1h-7z"
    "M20 3h9v1h-9zM0 4h7v1h-7zM11 4h7v1h-7zM20 4h8v1h-8zM0 5h2v1h-2zM17 5h2v1h-2z"
    "M20 5h2v1h-2zM1 6h8v1h-8zM10 6h8v1h-8zM20 6h2v1h-2zM2 7h7v1h-7zM10 7h7v1h-7z"
    "M20 7h2v1h-2z";

static const char kLogoNokia[] PROGMEM =
    "M31 0h3v1h-3zM39 0h2v1h-2zM44 0h8v1h-8zM55 0h2v1h-2zM60 0h3v1h-3zM65 0h2v1h-"
    "2zM72 0h4v1h-4zM31 1h4v1h-4zM39 1h2v1h-2zM43 1h10v1h-10zM55 1h2v1h-2zM59 1h3"
    "v1h-3zM65 1h2v1h-2zM71 1h6v1h-6zM31 2h5v1h-5zM39 2h2v1h-2zM43 2h2v1h-2zM51 2"
    "h2v1h-2zM55 2h2v1h-2zM58 2h3v1h-3zM65 2h2v1h-2zM70 2h3v1h-3zM75 2h2v1h-2zM31"
    " 3h2v1h-2zM34 3h2v1h-2zM39 3h2v1h-2zM43 3h2v1h-2zM51 3h2v1h-2zM55 3h5v1h-5zM"
    "65 3h2v1h-2zM69 3h4v1h-4zM75 3h3v1h-3zM31 4h2v1h-2zM35 4h2v1h-2zM39 4h2v1h-2"
    "zM43 4h2v1h-2zM51 4h2v1h-2zM55 4h5v1h-5zM65 4h2v1h-2zM69 4h2v1h-2zM75 4h2v1h"
    "-2zM31 5h2v1h-2zM36 5h2v1h-2zM39 5h2v1h-2zM43 5h2v1h-2zM51 5h2v1h-2zM55 5h2v"
    "1h-2zM58 5h3v1h-3zM65 5h2v1h-2zM68 5h10v1h-10zM31 6h2v1h-2zM37 6h4v1h-4zM43 "
    "6h10v1h-10zM55 6h2v1h-2zM59 6h3v1h-3zM65 6h2v1h-2zM68 6h10v1h-10zM31 7h2v1h-"
    "2zM38 7h3v1h-3zM44 7h8v1h-8zM55 7h2v1h-2zM60 7h3v1h-3zM65 7h2v1h-2zM68 7h2v1"
    "h-2zM76 7h2v1h-2z";

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

// scan sob demanda: [{"s":ssid,"r":rssi,"l":0|1}] com cache de 15 s;
// o botao da pagina manda ?f=1 pra forcar rescan na hora
static String scan_json(bool fresh) {
  uint32_t now = millis();
  if (!fresh && json_.length() && now - scan_ms_ < 15000) return json_;
  scan_ms_ = now;
  int n = WiFi.scanNetworks();  // AP_STA permite escanear com o AP de pe
  String out = "[";
  int usados = 0;
  for (int i = 0; i < n && usados < 12; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;  // ocultas entram pelo campo manual
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
    if (usados++) out += ',';
    out += "{\"s\":\"" + s + "\",\"r\":" + String(WiFi.RSSI(i)) + ",\"l\":" +
           ((WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "1" : "0") + "}";
  }
  out += "]";
  WiFi.scanDelete();
  json_ = out;
  return json_;
}

static void scan_route() {
  server_.send(200, "application/json", scan_json(server_.hasArg("f")));
}

static void raiz() {
  String html = FPSTR(kPage);
  html.replace("%LE%", FPSTR(kLogoEsp));
  html.replace("%LN%", FPSTR(kLogoNokia));
  html.replace("%AP%", ap_name_);
  html.replace("%MAC%", cliente_mac());
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
  server_.on("/scan", scan_route);
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
