#include "app_clock.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "alarm.h"
#include "clockfmt.h"
#include "i18n.h"
#include "sound.h"
#include "drivers/rtc.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Estilo Clock (Menu 11) do 3310: hora grande no centro. OK abre as opcoes
// {Alarme, Timer}. O alarme usa o slot unico do alarme:: (proxima ocorrencia
// de HH:MM, persiste na NVS); o timer conta minutos por millis e dispara o
// overlay TEMPO! — ambos tocam o toque padrao do sistema.
enum View : uint8_t { V_CLOCK, V_MENU, V_ALARM, V_TIMER, V_STOPWATCH };
static View view = V_CLOCK;
static uint8_t cur = 0;
static bool al_status_ = false;        // V_ALARM mostrando o alarme armado
static uint8_t al_h_, al_m_, al_field_;
static uint16_t tm_min_ = 5;           // minutos do editor do timer

// cronômetro (crescente): OK inicia/pausa, DOWN zera (parado), C volta ao menu.
// Estado por millis; segue contando mesmo se sair do app (como cronômetro real)
static bool sw_run_ = false;
static uint32_t sw_start_ = 0, sw_accum_ = 0;
static uint32_t sw_elapsed() { return sw_accum_ + (sw_run_ ? (millis() - sw_start_) : 0); }

static void on_enter() { view = V_CLOCK; }

// arma a proxima ocorrencia de HH:MM: hoje se ainda nao passou, senao amanha
static void alarm_arm() {
  rtc::DateTime dt;
  if (!rtc::now(dt)) return;
  uint8_t d = dt.day, mo = dt.month;
  uint16_t y = dt.year;
  if (al_h_ < dt.hour || (al_h_ == dt.hour && al_m_ <= dt.min)) {
    if (++d > days_in_month(y, mo)) { d = 1; if (++mo > 12) { mo = 1; } }
  }
  char lbl[6];
  hhmm_format(al_h_, al_m_, lbl);
  alarme::set(STR_ALARM, d, mo, al_h_, al_m_, lbl);
}

static void alarm_open() {
  view = V_ALARM;
  al_status_ = alarme::armed_at(al_h_, al_m_);
  if (!al_status_) {  // editor parte da hora atual
    rtc::DateTime dt;
    if (rtc::now(dt)) { al_h_ = dt.hour; al_m_ = dt.min; }
    al_field_ = 0;
  }
}

static bool input(Button b, BtnEvent e) {
  if (e != EV_PRESS) return false;
  switch (view) {
    case V_CLOCK:
      if (b == BTN_OK) { view = V_MENU; cur = 0; return true; }
      return false;  // C nao consumido → shell volta pro menu
    case V_MENU:
      if (b == BTN_UP)   { cur = (cur + 2) % 3; return true; }
      if (b == BTN_DOWN) { cur = (cur + 1) % 3; return true; }
      if (b == BTN_OK) {
        if (cur == 0) alarm_open();
        else if (cur == 1) view = V_TIMER;
        else view = V_STOPWATCH;
        return true;
      }
      view = V_CLOCK;  // C volta
      return true;
    case V_ALARM:
      if (!rtc::present()) { view = V_MENU; return true; }  // sem RTC nao ha alarme
      if (al_status_) {  // armado: so desligar
        if (b == BTN_OK) {
          alarme::clear();
          sound::play(sound::SND_CONFIRM);
        }
        view = V_MENU;
        return true;
      }
      if (b == BTN_UP || b == BTN_DOWN) {
        int8_t d = (b == BTN_UP) ? 1 : -1;
        if (al_field_ == 0) al_h_ = (uint8_t)((al_h_ + 24 + d) % 24);
        else al_m_ = (uint8_t)((al_m_ + 60 + d) % 60);
        return true;
      }
      if (b == BTN_OK) {
        if (al_field_ == 0) { al_field_ = 1; return true; }
        alarm_arm();
        sound::play(sound::SND_CONFIRM);
        view = V_MENU;
        return true;
      }
      // C: volta um campo; no primeiro, cancela
      if (al_field_ > 0) al_field_--;
      else view = V_MENU;
      return true;
    case V_TIMER:
      if (alarme::timer_left_s() > 0) {  // rodando: so desligar
        if (b == BTN_OK) {
          alarme::timer_cancel();
          sound::play(sound::SND_CONFIRM);
          return true;  // cai no editor no proximo render
        }
        view = V_MENU;  // C volta (timer segue contando)
        return true;
      }
      if (b == BTN_UP) { tm_min_ = tm_min_ % 99 + 1; return true; }
      if (b == BTN_DOWN) { tm_min_ = (tm_min_ + 97) % 99 + 1; return true; }
      if (b == BTN_OK) {
        alarme::timer_start(tm_min_);
        sound::play(sound::SND_CONFIRM);
        return true;  // fica na tela: vira o countdown
      }
      view = V_MENU;  // C volta
      return true;
    case V_STOPWATCH:
      if (b == BTN_OK) {
        if (sw_run_) { sw_accum_ += millis() - sw_start_; sw_run_ = false; }  // pausa
        else { sw_start_ = millis(); sw_run_ = true; }                        // inicia
        return true;
      }
      if (b == BTN_DOWN && !sw_run_) { sw_accum_ = 0; return true; }          // zera (parado)
      view = V_MENU;  // C volta (o cronômetro segue contando se estava rodando)
      return true;
  }
  return false;
}

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_3310_small);
  switch (view) {
    case V_CLOCK: {
      char hhmm[6];
      bool colon = ((millis() / 1000) % 2) == 0;
      rtc::DateTime dt;
      bool tem = rtc::now(dt);
      if (tem) hhmm_format(dt.hour, dt.min, hhmm);
      else clock_format(millis(), hhmm, &colon);

      // HH e MM em posição fixa; ':' só quando aceso -> piscar limpo (nada de
      // apagar por cima, que sobrava meio ponto)
      g.setFont(u8g2_font_VCR_OSD_tn);
      char hh[3] = {hhmm[0], hhmm[1], '\0'};
      char hhc[4] = {hhmm[0], hhmm[1], ':', '\0'};
      char mm[3] = {hhmm[3], hhmm[4], '\0'};
      int x = 42 - (int)g.getStrWidth(hhmm) / 2;
      g.drawStr(x, 30, hh);
      g.drawStr(x + (int)g.getStrWidth(hhc), 30, mm);
      if (colon) g.drawStr(x + (int)g.getStrWidth(hh), 30, ":");

      g.setFont(u8g2_font_3310_small);
      uint32_t left = alarme::timer_left_s();
      if (left) {  // timer rodando: regressivo discreto sob a hora
        char t[8];
        snprintf(t, sizeof(t), "%02u:%02u", (unsigned)(left / 60), (unsigned)(left % 60));
        g.drawStr(42 - (int)g.getStrWidth(t) / 2, 40, t);
      } else if (tem) {  // senão, a data (dia da semana + DD/MM) sob a hora
        char d[20];
        snprintf(d, sizeof(d), "%s %02u/%02u",
                 day_name(date_weekday(dt.year, dt.month, dt.day)), dt.day, dt.month);
        g.drawUTF8(42 - (int)g.getUTF8Width(d) / 2, 40, d);
      }
      nokia_ui::softkey(g, tr(STR_OPTIONS));
      break;
    }
    case V_MENU: {
      nokia_ui::text_bold_center(g, 8, tr(STR_APP_CLOCK));
      const char* items[] = {tr(STR_ALARM), tr(STR_TIMER), tr(STR_STOPWATCH)};
      for (uint8_t i = 0; i < 3; i++) {
        int y = 11 + i * 9;
        nokia_ui::list_row(g, y, 84, items[i], i == cur);
      }
      nokia_ui::softkey(g, tr(STR_SELECT));
      break;
    }
    case V_ALARM: {
      nokia_ui::text_bold_center(g, 8, tr(STR_ALARM));
      if (!rtc::present()) {
        g.drawUTF8(2, 24, tr(STR_NO_RTC));
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      char hhmm[6];
      hhmm_format(al_h_, al_m_, hhmm);
      if (al_status_) {  // armado: hora grande + Desligar
        g.setFont(u8g2_font_VCR_OSD_tn);
        g.drawStr(42 - (int)g.getStrWidth(hhmm) / 2, 33, hhmm);
        g.setFont(u8g2_font_3310_small);
        nokia_ui::softkey(g, tr(STR_TURN_OFF));
        break;
      }
      int x = 42 - (int)g.getStrWidth(hhmm) / 2;
      g.drawStr(x, 25, hhmm);
      char part[3] = {hhmm[al_field_ ? 3 : 0], hhmm[al_field_ ? 4 : 1], '\0'};
      nokia_ui::inv_str(g, al_field_ ? x + (int)g.getStrWidth("00:") : x, 25, part);
      nokia_ui::softkey(g, tr(al_field_ == 0 ? STR_OK : STR_SAVE));
      break;
    }
    case V_TIMER: {
      nokia_ui::text_bold_center(g, 8, tr(STR_TIMER));
      uint32_t left = alarme::timer_left_s();
      if (left) {  // rodando: regressivo grande + Desligar
        char t[8];
        snprintf(t, sizeof(t), "%02u:%02u", (unsigned)(left / 60), (unsigned)(left % 60));
        g.setFont(u8g2_font_VCR_OSD_tn);
        g.drawStr(42 - (int)g.getStrWidth(t) / 2, 33, t);
        g.setFont(u8g2_font_3310_small);
        nokia_ui::softkey(g, tr(STR_TURN_OFF));
        break;
      }
      char m[8];
      snprintf(m, sizeof(m), "%u", tm_min_);
      const char* minsuf = tr(STR_MIN_SUFFIX);
      int wm = (int)g.getStrWidth(m), wmin = (int)g.getUTF8Width(minsuf);
      int x = 42 - (wm + wmin) / 2;
      nokia_ui::inv_str(g, x, 25, m);
      g.drawUTF8(x + wm, 25, minsuf);
      nokia_ui::softkey(g, tr(STR_OK));
      break;
    }
    case V_STOPWATCH: {
      nokia_ui::text_bold_center(g, 8, tr(STR_STOPWATCH));
      uint32_t ms = sw_elapsed();
      char t[12];
      snprintf(t, sizeof(t), "%02u:%02u.%02u", (unsigned)(ms / 60000),
               (unsigned)((ms / 1000) % 60), (unsigned)((ms / 10) % 100));
      g.setFont(u8g2_font_7x13B_tr);
      g.drawStr(42 - (int)g.getStrWidth(t) / 2, 30, t);
      g.setFont(u8g2_font_3310_small);
      if (!sw_run_ && ms) nokia_ui::text_center(g, 40, tr(STR_RESET));
      nokia_ui::softkey(g, tr(sw_run_ ? STR_STOP : STR_START));
      break;
    }
  }
}
// animacao de selecao: ponteiros giram (12h15 -> 6h45) e voltam pro 10h08
static const unsigned char* const kAnim[] = {icon_clock_f1_bits,
                                             icon_clock_f2_bits, nullptr};
const App app_clock = {STR_APP_CLOCK, on_enter, nullptr, input, nullptr, render,
                       icon_clock_bits, kAnim};
