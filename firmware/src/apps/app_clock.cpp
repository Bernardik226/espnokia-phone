#include "app_clock.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "alarm.h"
#include "clockfmt.h"
#include "i18n.h"
#include "sound.h"
#include "timeprefs.h"
#include "drivers/rtc.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

// Estilo Clock (Menu 11) do 3310: hora grande no centro. OK abre {Alarme,
// Timer, Cronômetro}. O alarme usa o slot único do alarme:: (persiste na NVS);
// o timer (MM:SS) conta por millis e dispara o overlay TEMPO!. O botão UP é a
// tecla de AÇÃO da tela (bolinha no HUD): no relógio alterna 12/24h, no
// cronômetro zera.
enum View : uint8_t { V_CLOCK, V_MENU, V_ALARM, V_TIMER, V_STOPWATCH };
static View view = V_CLOCK;
static uint8_t cur = 0;
static bool up_held_ = false;          // UP (tecla de ação) pressionado agora
static bool al_status_ = false;        // V_ALARM mostrando o alarme armado
static uint8_t al_h_, al_m_, al_field_;
static uint8_t tm_min_ = 5, tm_sec_ = 0, tm_field_ = 0;  // editor MM:SS do timer

// cronômetro (crescente): OK inicia/pausa, UP zera, C volta ao menu.
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
  if (b == BTN_UP) up_held_ = (e == EV_PRESS);   // bolinha da tecla de ação
  if (e != EV_PRESS) return false;
  switch (view) {
    case V_CLOCK:
      if (b == BTN_OK) { view = V_MENU; cur = 0; return true; }
      if (b == BTN_UP) { timeprefs::set_fmt24(!timeprefs::fmt24()); return true; }  // ação: 12/24h
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
      if (b == BTN_UP || b == BTN_DOWN) {   // ajusta o campo ativo (MM ou SS)
        int8_t d = (b == BTN_UP) ? 1 : -1;
        if (tm_field_ == 0) tm_min_ = (uint8_t)((tm_min_ + 100 + d) % 100);   // 0..99
        else tm_sec_ = (uint8_t)((tm_sec_ + 60 + d) % 60);
        return true;
      }
      if (b == BTN_OK) {
        if (tm_field_ == 0) { tm_field_ = 1; return true; }   // MM -> SS
        alarme::timer_start_s((uint32_t)tm_min_ * 60 + tm_sec_);
        sound::play(sound::SND_CONFIRM);
        tm_field_ = 0;
        return true;  // fica na tela: vira o countdown
      }
      // C: volta um campo; no primeiro, sai pro menu
      if (tm_field_ > 0) tm_field_--;
      else view = V_MENU;
      return true;
    case V_STOPWATCH:
      if (b == BTN_OK) {
        if (sw_run_) { sw_accum_ += millis() - sw_start_; sw_run_ = false; }  // pausa
        else { sw_start_ = millis(); sw_run_ = true; }                        // inicia
        return true;
      }
      if (b == BTN_UP) { sw_accum_ = 0; sw_start_ = millis(); return true; }  // ação: zera
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
      bool pm = false;
      rtc::DateTime dt;
      bool tem = rtc::now(dt);
      if (tem) hhmm_format12(dt.hour, dt.min, timeprefs::fmt24(), hhmm, &pm);
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
      if (tem && !timeprefs::fmt24()) {   // marca AM/PM no topo direito
        const char* ap = pm ? "PM" : "AM";
        g.drawStr(82 - (int)g.getStrWidth(ap), 8, ap);
      }
      uint32_t left = alarme::timer_left_s();
      if (left) {  // timer rodando: regressivo discreto sob a hora
        char t[8];
        snprintf(t, sizeof(t), "%02u:%02u", (unsigned)(left / 60), (unsigned)(left % 60));
        g.drawStr(42 - (int)g.getStrWidth(t) / 2, 40, t);
      } else if (tem && timeprefs::show_date()) {  // data (dia + DD/MM), se ligada
        char d[20];
        snprintf(d, sizeof(d), "%s %02u/%02u",
                 day_name(date_weekday(dt.year, dt.month, dt.day)), dt.day, dt.month);
        g.drawUTF8(42 - (int)g.getUTF8Width(d) / 2, 40, d);
      }
      nokia_ui::softkey_action(g, tr(STR_OPTIONS), up_held_);   // UP: alterna 12/24h
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
      if (al_status_) {  // armado: hora grande + quanto falta + Desligar
        g.setFont(u8g2_font_VCR_OSD_tn);
        g.drawStr(42 - (int)g.getStrWidth(hhmm) / 2, 29, hhmm);
        g.setFont(u8g2_font_3310_small);
        rtc::DateTime dt;
        if (rtc::now(dt)) {  // faltam Xh Ym até a próxima ocorrência
          int rem = ((al_h_ * 60 + al_m_) - (dt.hour * 60 + dt.min) + 1440) % 1440;
          if (rem == 0) rem = 1440;
          char r[24];
          snprintf(r, sizeof(r), "%s %dh%02d", tr(STR_RINGS_IN), rem / 60, rem % 60);
          nokia_ui::text_center(g, 40, r);
        }
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
      // editor MM:SS (dois campos, mesmo estilo do alarme): OK avança MM->SS
      char t[6];
      snprintf(t, sizeof(t), "%02u:%02u", tm_min_, tm_sec_);
      int x = 42 - (int)g.getStrWidth(t) / 2;
      g.drawStr(x, 25, t);
      char part[3] = {t[tm_field_ ? 3 : 0], t[tm_field_ ? 4 : 1], '\0'};
      nokia_ui::inv_str(g, tm_field_ ? x + (int)g.getStrWidth("00:") : x, 25, part);
      nokia_ui::softkey(g, tr(tm_field_ == 0 ? STR_OK : STR_SAVE));
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
      if (ms) nokia_ui::text_center(g, 40, tr(STR_RESET));   // UP zera (a qualquer hora)
      nokia_ui::softkey_action(g, tr(sw_run_ ? STR_STOP : STR_START), up_held_);
      break;
    }
  }
}
// animacao de selecao: ponteiros giram (12h15 -> 6h45) e voltam pro 10h08
static const unsigned char* const kAnim[] = {icon_clock_f1_bits,
                                             icon_clock_f2_bits, nullptr};
const App app_clock = {STR_APP_CLOCK, on_enter, nullptr, input, nullptr, render,
                       icon_clock_bits, kAnim};
