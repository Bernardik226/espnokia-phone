#pragma once
#include <stdint.h>
#include "btnlogic.h"

// Contrato de app do NokiaOS. on_render recebe void* para manter o core puro
// (o glue passa U8G2*). Callbacks podem ser nullptr.
// name_id: StrId do nome no launcher (lib i18n) — o core so carrega o numero,
// quem renderiza traduz. icon: XBM 28x24 (convencao fixa do menu) ou nullptr;
// fica por ultimo para que apps sem icone nem precisem declara-lo.
struct App {
  uint8_t name_id;
  void (*on_enter)();
  void (*on_tick)(uint32_t now_ms);
  bool (*on_input)(Button b, BtnEvent e);  // true = consumiu o evento
  void (*on_exit)();
  void (*on_render)(void* gfx);
  const unsigned char* icon;
};
