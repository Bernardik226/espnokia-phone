#include "app_settings.h"
#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include "clockfmt.h"
#include "version.h"
#include "drivers/backlight.h"
#include "drivers/rtc.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Configuracoes em arvore rasa: V_ROOT lista as secoes; V_DISPLAY ajusta o
// backlight com preview ao vivo (so persiste no OK); V_DATETIME tem o toggle
// de sincronizacao via internet (NVS) e o acerto manual do RTC (V_DT_EDIT:
// UP/DOWN muda o campo invertido, OK avanca, C volta — como no 3310);
// V_WIFI e placeholder; V_ABOUT tem 3 paginas (UP/DOWN navega).
enum View : uint8_t { V_ROOT, V_DISPLAY, V_DATETIME, V_DT_EDIT, V_WIFI, V_ABOUT };
static const uint8_t kAboutPages = 3;
static View view = V_ROOT;
static uint8_t cur = 0;         // cursor da lista corrente
static uint8_t about_page = 0;  // 0..kAboutPages-1

static const char* kRoot[] = {"Display", "Data e hora", "Wifi", "Sobre"};
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

// editor de data/hora: buffer proprio, so grava no RTC no Salvar
static rtc::DateTime edit_;
static uint8_t dt_field_ = 0;  // 0=hora 1=min 2=dia 3=mes 4=ano
static const char* kDows[] = {"Domingo", "Segunda", "Terca",  "Quarta",
                              "Quinta",  "Sexta",   "Sabado"};

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
        else if (cur == 2) { view = V_WIFI; }
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
      if (b == BTN_UP || b == BTN_DOWN) { cur ^= 1; return true; }  // 2 itens
      if (b == BTN_OK) {
        if (cur == 0) {
          ntp_sync_set(!ntp_sync_);
          if (!ntp_sync_) dt_edit_open();  // desligou o auto → acerta na hora
        } else {
          dt_edit_open();
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
    case V_WIFI:
      view = V_ROOT; cur = 2;  // qualquer tecla volta
      return true;
    case V_ABOUT:
      if (b == BTN_UP) { about_page = (about_page + kAboutPages - 1) % kAboutPages; return true; }
      if (b == BTN_DOWN) { about_page = (about_page + 1) % kAboutPages; return true; }
      view = V_ROOT; cur = 3;
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
    if (top + i == sel) {
      g.drawBox(0, y, 84, 9);
      g.setDrawColor(0);
      g.drawStr(3, y + 8, items[top + i]);
      g.setDrawColor(1);
    } else {
      g.drawStr(3, y + 8, items[top + i]);
    }
  }
}

// campo selecionado do editor: texto invertido sobre uma caixa
static void inv_str(U8G2& g, int x, int baseline, const char* s) {
  g.drawBox(x - 1, baseline - 7, (int)g.getStrWidth(s) + 2, 9);
  g.setDrawColor(0);
  g.drawStr(x, baseline, s);
  g.setDrawColor(1);
}

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_3310_small);
  switch (view) {
    case V_ROOT:
      draw_list(g, "Config", kRoot, kRootCount, cur);
      nokia_ui::softkey(g, "Selecionar");
      break;
    case V_DISPLAY: {
      const char* names[backlight::kLevels];
      for (uint8_t i = 0; i < backlight::kLevels; i++) names[i] = backlight::name(i);
      draw_list(g, "Luz do visor", names, backlight::kLevels, cur);
      nokia_ui::softkey(g, "OK");
      break;
    }
    case V_DATETIME: {
      nokia_ui::text_bold_center(g, 8, "Data e hora");
      const char* items[] = {"Sincronizar", "Acertar agora"};
      for (uint8_t i = 0; i < 2; i++) {
        int y = 11 + i * 9;
        bool sel = (cur == i);
        if (sel) { g.drawBox(0, y, 84, 9); g.setDrawColor(0); }
        g.drawStr(3, y + 8, items[i]);
        if (i == 0) {  // checkbox do toggle, a direita
          g.drawFrame(73, y + 2, 7, 7);
          if (ntp_sync_) {
            g.drawLine(74, y + 5, 75, y + 7);
            g.drawLine(75, y + 7, 78, y + 3);
          }
        }
        if (sel) g.setDrawColor(1);
      }
      if (!rtc::present()) g.drawStr(3, 38, "RTC ausente");
      nokia_ui::softkey(g, cur == 0 ? "Mudar" : "Selecionar");
      break;
    }
    case V_DT_EDIT: {
      nokia_ui::text_bold_center(g, 8, "Acertar");
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
      const char* dow = kDows[date_weekday(edit_.year, edit_.month, edit_.day)];
      g.drawStr(42 - (int)g.getStrWidth(dow) / 2, 38, dow);
      // campo em edicao por cima, invertido
      int sep_t = (int)g.getStrWidth(":"), sep_d = (int)g.getStrWidth("/");
      switch (dt_field_) {
        case 0: inv_str(g, tx, 19, hh); break;
        case 1: inv_str(g, tx + (int)g.getStrWidth(hh) + sep_t, 19, mi); break;
        case 2: inv_str(g, dx, 30, dd); break;
        case 3: inv_str(g, dx + (int)g.getStrWidth(dd) + sep_d, 30, mo); break;
        case 4:
          inv_str(g, dx + (int)g.getStrWidth(dd) + (int)g.getStrWidth(mo) + 2 * sep_d, 30, yr);
          break;
      }
      nokia_ui::softkey(g, dt_field_ < 4 ? "OK" : "Salvar");
      break;
    }
    case V_WIFI:
      nokia_ui::text_bold_center(g, 8, "Wifi");
      g.drawStr(2, 19, "Status: offline");
      g.drawStr(2, 28, "Config em breve");
      nokia_ui::softkey(g, "Voltar");
      break;
    case V_ABOUT: {
      char buf[20];
      if (about_page == 0) {  // marca: emblema eN + wordmark
        g.drawXBMP(24, 0, ESPNOKIA_EMBLEM_W, ESPNOKIA_EMBLEM_H, espnokia_emblem_bits);
        g.drawXBMP(2, 22, ESPNOKIA_LOGO_W, ESPNOKIA_LOGO_H, espnokia_logo_bits);
      } else if (about_page == 1) {  // descricao
        nokia_ui::text_bold_center(g, 8, "Sobre");
        g.drawStr(2, 19, "Nokia OS fake");
        g.drawStr(2, 28, "ESP32 + 5110");
      } else {  // specs do sistema
        nokia_ui::text_bold_center(g, 8, "Sistema");
        snprintf(buf, sizeof(buf), "fw %s", ESPNOKIA_FW_VERSION);
        g.drawStr(2, 19, buf);
        snprintf(buf, sizeof(buf), "RAM livre %uKB", (unsigned)(ESP.getFreeHeap() / 1024));
        g.drawStr(2, 28, buf);
        g.drawStr(2, 37, "CPU 240MHz");
      }
      snprintf(buf, sizeof(buf), "%u/%u", about_page + 1, kAboutPages);
      g.drawStr(82 - (int)g.getStrWidth(buf), 37, buf);
      nokia_ui::softkey(g, "Voltar");
      break;
    }
  }
}

const App app_settings = {"Config", on_enter, nullptr, input, nullptr, render,
                          icon_settings_bits};
