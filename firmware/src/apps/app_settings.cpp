#include "app_settings.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "version.h"
#include "drivers/backlight.h"
#include "ui/assets.h"
#include "ui/nokia_ui.h"

// Configuracoes em arvore rasa: V_ROOT lista as secoes; V_DISPLAY ajusta o
// backlight com preview ao vivo (so persiste no OK); V_WIFI e placeholder;
// V_ABOUT tem 3 paginas (UP/DOWN navega): marca, descricao, sistema.
enum View : uint8_t { V_ROOT, V_DISPLAY, V_WIFI, V_ABOUT };
static const uint8_t kAboutPages = 3;
static View view = V_ROOT;
static uint8_t cur = 0;         // cursor da lista corrente
static uint8_t about_page = 0;  // 0..kAboutPages-1

static const char* kRoot[] = {"Display", "Wifi", "Sobre"};
static const uint8_t kRootCount = sizeof(kRoot) / sizeof(kRoot[0]);

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
        else if (cur == 1) { view = V_WIFI; }
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
    case V_WIFI:
      view = V_ROOT; cur = 1;  // qualquer tecla volta
      return true;
    case V_ABOUT:
      if (b == BTN_UP) { about_page = (about_page + kAboutPages - 1) % kAboutPages; return true; }
      if (b == BTN_DOWN) { about_page = (about_page + 1) % kAboutPages; return true; }
      view = V_ROOT; cur = 2;
      return true;
  }
  return false;
}

// lista curta (cabe inteira) com barra invertida, como no launcher
static void draw_list(U8G2& g, const char* title, const char* const* items,
                      uint8_t n, uint8_t sel) {
  nokia_ui::text_bold_center(g, 8, title);
  for (uint8_t i = 0; i < n; i++) {
    int y = 11 + i * 9;
    if (i == sel) {
      g.drawBox(0, y, 84, 9);
      g.setDrawColor(0);
      g.drawStr(3, y + 7, items[i]);
      g.setDrawColor(1);
    } else {
      g.drawStr(3, y + 7, items[i]);
    }
  }
}

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_nokiafc22_tr);
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
