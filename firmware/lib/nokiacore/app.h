#pragma once
#include <stdint.h>
#include "btnlogic.h"

// Contrato de app do NokiaOS. on_render recebe void* para manter o core puro
// (o glue passa U8G2*). Callbacks podem ser nullptr.
// name_id: StrId do nome no launcher (lib i18n) — o core so carrega o numero,
// quem renderiza traduz. icon: XBM 28x24 (convencao fixa do menu) ou nullptr.
// anim: frames XBM 28x24 terminados em nullptr — o menu toca a sequencia uma
// vez quando o app e selecionado e descansa no icon. Os opcionais ficam por
// ultimo para que apps sem icone/animacao nem precisem declara-los.
struct App {
  uint8_t name_id;
  void (*on_enter)();
  void (*on_tick)(uint32_t now_ms);
  bool (*on_input)(Button b, BtnEvent e);  // true = consumiu o evento
  void (*on_exit)();
  void (*on_render)(void* gfx);
  const unsigned char* icon;
  const unsigned char* const* anim;
};
