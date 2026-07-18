#include "app_settings.h"
#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include "clockfmt.h"
#include "i18n.h"
#include "sound.h"
#include "timeprefs.h"
#include "version.h"
#include "drivers/backlight.h"
#include "drivers/buzzer.h"
#include "drivers/rtc.h"
#include "net/conn.h"
#include "net/wifi.h"
#include "qrcode.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"
#include "ui/wallpaper.h"

// Configuracoes em arvore rasa: V_ROOT lista as secoes; V_DISPLAY ajusta o
// backlight com preview ao vivo (so persiste no OK); V_DATETIME tem o toggle
// de sincronizacao via internet (NVS) e o acerto manual do RTC (V_DT_EDIT:
// UP/DOWN muda o campo invertido, OK avanca, C volta — como no 3310);
// V_WIFI mostra rede/IP (ou o AP do modo config) e abre V_WIFI_MENU com
// Esquecer/Trocar rede (ambos reiniciam); V_VOL ajusta o volume com demo ao
// vivo (so persiste no OK, como o backlight); V_LANG troca o idioma do
// sistema (vale na hora e persiste); V_ABOUT tem 3 paginas (UP/DOWN).
enum View : uint8_t { V_ROOT, V_DISPLAY, V_DATETIME, V_DT_EDIT, V_CONN,
                      V_WIFI, V_WIFI_MENU, V_QR, V_VOL, V_LANG, V_ABOUT, V_WALL };
static const uint8_t kAboutPages = 3;
static View view = V_ROOT;
static uint8_t cur = 0;         // cursor da lista corrente
static uint8_t about_page = 0;  // 0..kAboutPages-1

static const StrId kRoot[] = {STR_DISPLAY, STR_DATETIME, STR_CONNECTIONS,
                              STR_VOLUME, STR_LANGUAGE, STR_WALLPAPER, STR_ABOUT};
static const uint8_t kRootCount = sizeof(kRoot) / sizeof(kRoot[0]);

// toggle "Sincronizar" persistido na NVS; a F2 le pra decidir NTP no boot.
// padrao desligado: sem WiFi configurado o acerto manual e o caminho real
static Preferences prefs_;
static bool prefs_open_ = false;
static bool ntp_sync_ = false;
static void prefs_ensure() {
  if (prefs_open_) return;
  prefs_.begin("espnokia");
  ntp_sync_ = prefs_.getBool("ntp", false);
  prefs_open_ = true;
}
bool settings_ntp_sync_enabled() {
  prefs_ensure();
  return ntp_sync_;
}
static void ntp_sync_set(bool v) {
  prefs_ensure();
  ntp_sync_ = v;
  prefs_.putBool("ntp", v);
}
void settings_load_lang() {
  prefs_ensure();
  i18n_set((Lang)prefs_.getUChar("lang", LANG_PT));
}
static void lang_save(Lang l) {
  prefs_ensure();
  i18n_set(l);
  prefs_.putUChar("lang", (uint8_t)l);
}

// editor de data/hora: buffer proprio, so grava no RTC no Salvar
static rtc::DateTime edit_;
static uint8_t dt_field_ = 0;  // 0=hora 1=min 2=dia 3=mes 4=ano
static void dt_edit_open() {
  if (!rtc::now(edit_)) return;  // sem RTC nao ha o que acertar
  dt_field_ = 0;
  view = V_DT_EDIT;
}

static void dt_adjust(int8_t d) {
  switch (dt_field_) {
    case 0: edit_.hour = (uint8_t)((edit_.hour + 24 + d) % 24); break;
    case 1: edit_.min = (uint8_t)((edit_.min + 60 + d) % 60); break;
    case 2: {
      uint8_t dim = days_in_month(edit_.year, edit_.month);
      edit_.day = (uint8_t)((edit_.day - 1 + dim + d) % dim + 1);
      break;
    }
    case 3: edit_.month = (uint8_t)((edit_.month - 1 + 12 + d) % 12 + 1); break;
    case 4: edit_.year = (uint16_t)(2000 + (edit_.year - 2000 + 100 + d) % 100); break;
  }
  // mudar mes/ano pode invalidar o dia (ex.: 31/01 → fevereiro)
  uint8_t dim = days_in_month(edit_.year, edit_.month);
  if (edit_.day > dim) edit_.day = dim;
}

static void on_enter() {
  view = V_ROOT;
  cur = 0;
  about_page = 0;
}

static bool input(Button b, BtnEvent e) {
  if (e != EV_PRESS) return false;
  switch (view) {
    case V_ROOT:
      if (b == BTN_UP) { cur = (cur + kRootCount - 1) % kRootCount; return true; }
      if (b == BTN_DOWN) { cur = (cur + 1) % kRootCount; return true; }
      if (b == BTN_OK) {
        if (cur == 0) { view = V_DISPLAY; cur = backlight::level(); }
        else if (cur == 1) { view = V_DATETIME; cur = 0; }
        else if (cur == 2) { view = V_CONN; cur = 0; }
        else if (cur == 3) { view = V_VOL; cur = sound::muted() ? 0 : sound::volume() + 1; }
        else if (cur == 4) { view = V_LANG; cur = (uint8_t)i18n_lang(); }
        else if (cur == 5) { view = V_WALL; cur = wallpaper::current(); }
        else { view = V_ABOUT; about_page = 0; }
        return true;
      }
      return false;  // C nao consumido → shell volta pro menu
    case V_DISPLAY:
      if (b == BTN_UP || b == BTN_DOWN) {  // preview ao vivo, nao persiste
        cur = (b == BTN_UP) ? (cur + backlight::kLevels - 1) % backlight::kLevels
                            : (cur + 1) % backlight::kLevels;
        backlight::apply(cur);
        return true;
      }
      if (b == BTN_OK) { backlight::save(cur); view = V_ROOT; cur = 0; return true; }
      // C cancela: reverte pro nivel persistido
      backlight::apply(backlight::level());
      view = V_ROOT; cur = 0;
      return true;
    case V_DATETIME:
      if (b == BTN_UP)   { cur = (cur + 3) % 4; return true; }  // 4 itens
      if (b == BTN_DOWN) { cur = (cur + 1) % 4; return true; }
      if (b == BTN_OK) {
        if (cur == 0) {
          ntp_sync_set(!ntp_sync_);
          if (!ntp_sync_) dt_edit_open();  // desligou o auto → acerta na hora
        } else if (cur == 1) {
          dt_edit_open();
        } else if (cur == 2) {
          timeprefs::set_show_date(!timeprefs::show_date());  // data no menu inicial
        } else {
          timeprefs::set_fmt24(!timeprefs::fmt24());  // formato do sistema: AM/PM(12h) vs 24h
        }
        return true;
      }
      view = V_ROOT; cur = 1;  // C volta
      return true;
    case V_DT_EDIT:
      if (b == BTN_UP) { dt_adjust(+1); return true; }
      if (b == BTN_DOWN) { dt_adjust(-1); return true; }
      if (b == BTN_OK) {
        if (dt_field_ < 4) { dt_field_++; return true; }
        edit_.sec = 0;  // acerto manual zera os segundos, como no 3310
        edit_.dow = date_weekday(edit_.year, edit_.month, edit_.day);
        rtc::write(edit_);
        view = V_DATETIME; cur = 1;
        return true;
      }
      // C: volta um campo; no primeiro, cancela sem gravar
      if (dt_field_ > 0) dt_field_--;
      else { view = V_DATETIME; cur = 1; }
      return true;
    case V_CONN:
      if (b == BTN_UP || b == BTN_DOWN) { cur ^= 1; return true; }  // 2 itens
      if (b == BTN_OK) {
        if (cur == 0) view = V_WIFI;     // WiFi/Servidor: status + reconfig
        else view = V_QR;                // QR pra logar no painel
        return true;
      }
      view = V_ROOT; cur = 2;  // C volta pro root
      return true;
    case V_WIFI:
      if (b == BTN_OK && !wifi::provisioning()) { view = V_WIFI_MENU; cur = 0; return true; }
      view = V_CONN; cur = 0;  // C (ou OK no modo config) volta pro submenu
      return true;
    case V_QR:
      view = V_CONN; cur = 1;  // qualquer tecla fecha o QR
      return true;
    case V_WIFI_MENU:
      if (b == BTN_UP || b == BTN_DOWN) { cur ^= 1; return true; }  // 2 itens
      if (b == BTN_OK) {
        if (cur == 0) wifi::forget();   // ambos reiniciam o aparelho
        else wifi::reconfigure();       // (nunca retornam)
        return true;
      }
      view = V_WIFI;  // C volta pro status
      return true;
    case V_VOL:
      // 4 itens: 0=Mudo, 1/2/3 = baixo/medio/alto (volume 0/1/2)
      if (b == BTN_UP || b == BTN_DOWN) {  // demo ao vivo, nao persiste
        cur = (b == BTN_UP) ? (cur + 3) % 4 : (cur + 1) % 4;
        buzzer::set_mute(cur == 0);
        if (cur > 0) { buzzer::set_volume(cur - 1); sound::play(sound::SND_KEY); }
        return true;
      }
      if (b == BTN_OK) {
        sound::set_muted(cur == 0);
        if (cur > 0) sound::set_volume(cur - 1);
        view = V_ROOT; cur = 3; return true;
      }
      // C cancela: reverte pro estado persistido (mute + volume)
      buzzer::set_mute(sound::muted());
      buzzer::set_volume(sound::volume());
      view = V_ROOT; cur = 3;
      return true;
    case V_WALL:
      if (b == BTN_UP)   { cur = (cur + wallpaper::count() - 1) % wallpaper::count(); return true; }
      if (b == BTN_DOWN) { cur = (cur + 1) % wallpaper::count(); return true; }
      if (b == BTN_OK)   { wallpaper::set(cur); view = V_ROOT; cur = 5; return true; }
      view = V_ROOT; cur = 5;   // C cancela (nao salva)
      return true;
    case V_LANG:
      if (b == BTN_UP) { cur = (cur + LANG_COUNT - 1) % LANG_COUNT; return true; }
      if (b == BTN_DOWN) { cur = (cur + 1) % LANG_COUNT; return true; }
      if (b == BTN_OK) { lang_save((Lang)cur); view = V_ROOT; cur = 4; return true; }
      view = V_ROOT; cur = 4;  // C cancela
      return true;
    case V_ABOUT:
      if (b == BTN_UP) { about_page = (about_page + kAboutPages - 1) % kAboutPages; return true; }
      if (b == BTN_DOWN) { about_page = (about_page + 1) % kAboutPages; return true; }
      view = V_ROOT; cur = 5;
      return true;
  }
  return false;
}

// lista com barra invertida, como no launcher; janela de 3 itens rola
// pra caber com a softkey (o 3310 tambem rolava as listas de config)
static void draw_list(U8G2& g, const char* title, const char* const* items,
                      uint8_t n, uint8_t sel) {
  nokia_ui::text_bold_center(g, 8, title);
  const uint8_t kVis = 3;
  uint8_t top = sel >= kVis ? (uint8_t)(sel - kVis + 1) : 0;
  for (uint8_t i = 0; i < kVis && top + i < n; i++) {
    int y = 11 + i * 9;
    nokia_ui::list_row(g, y, 84, items[top + i], top + i == sel);
  }
}

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_3310_small);
  switch (view) {
    case V_ROOT: {
      const char* items[kRootCount];
      for (uint8_t i = 0; i < kRootCount; i++) items[i] = tr(kRoot[i]);
      draw_list(g, tr(STR_APP_SETTINGS), items, kRootCount, cur);
      nokia_ui::softkey(g, tr(STR_SELECT));
      break;
    }
    case V_DISPLAY: {
      const char* names[] = {tr(STR_DISP_OFF), tr(STR_DISP_MED), tr(STR_DISP_HIGH)};
      draw_list(g, tr(STR_BACKLIGHT), names, backlight::kLevels, cur);
      nokia_ui::softkey(g, tr(STR_OK));
      break;
    }
    case V_DATETIME: {
      nokia_ui::text_bold_center(g, 8, tr(STR_DATETIME));
      const char* items[] = {tr(STR_SYNC), tr(STR_SET_NOW), tr(STR_SHOW_DATE),
                             tr(STR_AMPM)};
      const uint8_t n = 4, kVis = 3;
      uint8_t top = cur >= kVis ? (uint8_t)(cur - kVis + 1) : 0;  // janela rola
      for (uint8_t i = 0; i < kVis && top + i < n; i++) {
        uint8_t idx = top + i;
        int y = 11 + i * 9;
        bool sel = (cur == idx);
        if (sel) { g.drawBox(0, y, 84, 9); g.setDrawColor(0); }
        g.drawUTF8(3, y + 8, items[idx]);
        if (idx != 1) {  // checkbox dos toggles (Sincronizar / Mostrar data / AM/PM); idx 1 = ação
          bool on = idx == 0 ? ntp_sync_
                  : idx == 2 ? timeprefs::show_date()
                             : !timeprefs::fmt24();  // AM/PM marcado = 12h
          g.drawFrame(73, y + 2, 7, 7);
          if (on) {
            g.drawLine(74, y + 5, 75, y + 7);
            g.drawLine(75, y + 7, 78, y + 3);
          }
        }
        if (sel) g.setDrawColor(1);
      }
      nokia_ui::softkey(g, tr(cur == 1 ? STR_SELECT : STR_CHANGE));
      break;
    }
    case V_DT_EDIT: {
      nokia_ui::text_bold_center(g, 8, tr(STR_SET_NOW));
      char hh[3], mi[3], dd[3], mo[3], yr[5];
      snprintf(hh, sizeof(hh), "%02u", edit_.hour);
      snprintf(mi, sizeof(mi), "%02u", edit_.min);
      snprintf(dd, sizeof(dd), "%02u", edit_.day);
      snprintf(mo, sizeof(mo), "%02u", edit_.month);
      snprintf(yr, sizeof(yr), "%04u", edit_.year);
      char tline[6], dline[11];
      snprintf(tline, sizeof(tline), "%s:%s", hh, mi);
      snprintf(dline, sizeof(dline), "%s/%s/%s", dd, mo, yr);
      int tx = 42 - (int)g.getStrWidth(tline) / 2;
      int dx = 42 - (int)g.getStrWidth(dline) / 2;
      g.drawStr(tx, 19, tline);
      g.drawStr(dx, 30, dline);
      // dia da semana derivado da data, so leitura
      const char* dow = day_name(date_weekday(edit_.year, edit_.month, edit_.day));
      g.drawUTF8(42 - (int)g.getUTF8Width(dow) / 2, 38, dow);
      // campo em edicao por cima, invertido
      int sep_t = (int)g.getStrWidth(":"), sep_d = (int)g.getStrWidth("/");
      switch (dt_field_) {
        case 0: nokia_ui::inv_str(g, tx, 19, hh); break;
        case 1: nokia_ui::inv_str(g, tx + (int)g.getStrWidth(hh) + sep_t, 19, mi); break;
        case 2: nokia_ui::inv_str(g, dx, 30, dd); break;
        case 3: nokia_ui::inv_str(g, dx + (int)g.getStrWidth(dd) + sep_d, 30, mo); break;
        case 4:
          nokia_ui::inv_str(g, dx + (int)g.getStrWidth(dd) + (int)g.getStrWidth(mo) + 2 * sep_d, 30, yr);
          break;
      }
      nokia_ui::softkey(g, tr(dt_field_ < 4 ? STR_OK : STR_SAVE));
      break;
    }
    case V_CONN: {
      const char* items[] = {tr(STR_WIFI_SERVER), tr(STR_DEVICE_QR)};
      draw_list(g, tr(STR_CONNECTIONS), items, 2, cur);
      nokia_ui::softkey(g, tr(STR_SELECT));
      break;
    }
    case V_WIFI: {
      char ip[16], buf2[20];
      wifi::ip_str(ip, sizeof(ip));
      if (wifi::provisioning()) {  // AP de configuracao no ar
        nokia_ui::text_bold_center(g, 8, tr(STR_CONFIG_MODE));
        snprintf(buf2, sizeof(buf2), "%.18s", wifi::ssid());
        g.drawUTF8(2, 19, buf2);
        snprintf(buf2, sizeof(buf2), tr(STR_PASS_FMT), wifi::ap_pass());
        g.drawUTF8(2, 28, buf2);
        g.drawStr(2, 37, ip);
        nokia_ui::softkey(g, tr(STR_BACK));
      } else {
        nokia_ui::text_bold_center(g, 8, tr(STR_WIFI));
        snprintf(buf2, sizeof(buf2), "%.18s", wifi::ssid());
        g.drawUTF8(2, 19, buf2);
        if (wifi::connected()) g.drawStr(2, 28, ip);
        else g.drawUTF8(2, 28, tr(STR_CONNECTING));
        const char* u = conn::server_url();       // host do server configurado
        const char* host = strstr(u, "://");
        snprintf(buf2, sizeof(buf2), "%.18s", host ? host + 3 : u);
        g.drawStr(2, 37, buf2);
        nokia_ui::softkey(g, tr(STR_OPTIONS));
      }
      break;
    }
    case V_WIFI_MENU: {
      const char* items[] = {tr(STR_FORGET_NET), tr(STR_SWITCH_NET)};
      draw_list(g, tr(STR_WIFI_SERVER), items, 2, cur);
      nokia_ui::softkey(g, tr(STR_OK));
      break;
    }
    case V_QR: {
      // QR do "endereco do server/#k=chave": a camera do celular abre o
      // dashboard ja logado. 1 px por modulo, centralizado (quiet zone = o
      // fundo claro do LCD). Versao escolhida pelo tamanho do link.
      // pior caso: url ate 90 (maxlength do form) + "/#k=" (4) + key ate 39
      // (key_[40] em conn.cpp) + nul = ate 134 chars; 160 da folga.
      char link[160];
      conn::pair_link(link, sizeof(link));
      static uint8_t qrbuf[256];  // qrcode_getBufferSize(6) = 211, com folga
      QRCode qr;
      size_t L = strlen(link);
      uint8_t ver = L <= 78 ? 4 : (L <= 106 ? 5 : 6);
      qrcode_initText(&qr, qrbuf, ver, ECC_LOW, link);
      int x0 = (84 - qr.size) / 2, y0 = (48 - qr.size) / 2;
      if (y0 < 0) y0 = 0;
      for (uint8_t y = 0; y < qr.size; y++)
        for (uint8_t x = 0; x < qr.size; x++)
          if (qrcode_getModule(&qr, x, y)) g.drawPixel(x0 + x, y0 + y);
      break;
    }
    case V_VOL: {
      const char* items[] = {tr(STR_MUTE), tr(STR_VOL_LOW), tr(STR_VOL_MED), tr(STR_VOL_HIGH)};
      draw_list(g, tr(STR_VOLUME), items, 4, cur);
      nokia_ui::softkey(g, tr(STR_OK));
      break;
    }
    case V_WALL: {
      wallpaper::draw_at(g, cur);            // preview em tela cheia do selecionado
      g.setDrawColor(0); g.drawBox(0, 40, 84, 8); g.setDrawColor(1);
      g.drawHLine(0, 40, 84);                // faixa do nome no rodapé
      nokia_ui::text_bold_center(g, 47, wallpaper::name(cur));
      break;
    }
    case V_LANG: {
      const char* items[LANG_COUNT];
      for (uint8_t i = 0; i < LANG_COUNT; i++) items[i] = lang_name((Lang)i);
      draw_list(g, tr(STR_LANGUAGE), items, LANG_COUNT, cur);
      nokia_ui::softkey(g, tr(STR_OK));
      break;
    }
    case V_ABOUT: {
      char buf[20];
      if (about_page == 0) {  // marca: emblema eN + wordmark
        g.drawXBMP(24, 0, ESPNOKIA_EMBLEM_W, ESPNOKIA_EMBLEM_H, espnokia_emblem_bits);
        g.drawXBMP(2, 22, ESPNOKIA_LOGO_W, ESPNOKIA_LOGO_H, espnokia_logo_bits);
      } else if (about_page == 1) {  // descricao
        nokia_ui::text_bold_center(g, 8, tr(STR_ABOUT));
        // "Nokia OS fake" e tagline/flavor (como "CLAWD" em claudemodel.cpp);
        // "ESP32 + 5110" e nome de peca/hardware. Intencionalmente fixos em
        // todo idioma, nao sao texto de UI a traduzir.
        g.drawStr(2, 19, "Nokia OS fake");
        g.drawStr(2, 28, "ESP32 + 5110");
      } else {  // specs do sistema
        nokia_ui::text_bold_center(g, 8, tr(STR_SYSTEM));
        snprintf(buf, sizeof(buf), "fw %s", ESPNOKIA_FW_VERSION);
        g.drawStr(2, 19, buf);
        snprintf(buf, sizeof(buf), tr(STR_RAM_FREE_FMT), (unsigned)(ESP.getFreeHeap() / 1024));
        g.drawUTF8(2, 28, buf);
        // "CPU 240MHz": spec fixa (frequencia do ESP32), nome proprio de
        // hardware, intencionalmente nao traduzida.
        g.drawStr(2, 37, "CPU 240MHz");
      }
      snprintf(buf, sizeof(buf), "%u/%u", about_page + 1, kAboutPages);
      g.drawStr(82 - (int)g.getStrWidth(buf), 37, buf);
      nokia_ui::softkey(g, tr(STR_BACK));
      break;
    }
  }
}

// animacao de selecao: engrenagem gira dente a dente (meio passo por frame)
static const unsigned char* const kAnim[] = {
    icon_settings_f1_bits, icon_settings_bits, icon_settings_f1_bits, nullptr};
const App app_settings = {STR_APP_SETTINGS, on_enter, nullptr, input, nullptr, render,
                          icon_settings_bits, kAnim};
