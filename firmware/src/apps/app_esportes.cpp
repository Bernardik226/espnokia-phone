#include "app_esportes.h"
#include <U8g2lib.h>
#include "app_copa.h"
#include "app_futebol.h"
#include "i18n.h"
#include "ui/assets.h"
#include "ui/menu_view.h"

// Categoria do launcher: o shell nao conhece submenu, entao Esportes e um
// app-conteiner — desenha o mesmo carrossel do launcher (menu_view) sobre os
// apps internos e, com um sub-app aberto, delega tick/input/render pra ele.
// Input que o sub-app nao consome segue a regra do shell: C fecha o sub e
// volta ao carrossel; C no carrossel nao e consumido → o shell fecha a
// categoria.

static const App* const kSub[] = {&app_futebol, &app_copa};
static const uint8_t kN = sizeof(kSub) / sizeof(kSub[0]);

static const App* sub_ = nullptr;  // sub-app aberto; nullptr = carrossel
static uint8_t cur_ = 0;

static void fecha_sub() {
  if (sub_ && sub_->on_exit) sub_->on_exit();
  sub_ = nullptr;
}

static void on_enter() {
  sub_ = nullptr;
  cur_ = 0;
}

static void on_exit() { fecha_sub(); }

static void tick(uint32_t now_ms) {
  if (sub_ && sub_->on_tick) sub_->on_tick(now_ms);
}

static bool input(Button b, BtnEvent e) {
  if (sub_) {
    if (sub_->on_input && sub_->on_input(b, e)) return true;
    if (e != EV_PRESS) return true;
    if (b == BTN_C) fecha_sub();
    return true;  // demais teclas nao navegam (igual ao shell em APP)
  }
  if (e != EV_PRESS) return false;
  if (b == BTN_UP) { cur_ = (cur_ + kN - 1) % kN; return true; }
  if (b == BTN_DOWN) { cur_ = (cur_ + 1) % kN; return true; }
  if (b == BTN_OK) {
    sub_ = kSub[cur_];
    if (sub_->on_enter) sub_->on_enter();
    return true;
  }
  return false;  // C nao consumido → shell fecha a categoria
}

static void render(void* gfx) {
  if (sub_) {
    if (sub_->on_render) sub_->on_render(gfx);
    return;
  }
  menu_view_draw(*(U8G2*)gfx, *kSub[cur_], kN, cur_);
}

// animacao de selecao: a bola gira 72° (simetria do pentagono fecha o loop)
static const unsigned char* const kAnim[] = {icon_esportes_f1_bits,
                                             icon_esportes_f2_bits, nullptr};
const App app_esportes = {STR_APP_SPORTS, on_enter, tick, input, on_exit,
                          render, icon_esportes_bits, kAnim};
