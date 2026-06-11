#pragma once
#include <stdint.h>
#include "btnlogic.h"

// Contrato de app do NokiaOS. on_render recebe void* para manter o core puro
// (o glue passa U8G2*). Callbacks podem ser nullptr.
// icon: XBM 28x24 (convencao fixa do menu) ou nullptr; fica por ultimo para
// que apps sem icone nem precisem declara-lo (value-init = nullptr).
struct App {
  const char* name;
  void (*on_enter)();
  void (*on_tick)(uint32_t now_ms);
  bool (*on_input)(Button b, BtnEvent e);  // true = consumiu o evento
  void (*on_exit)();
  void (*on_render)(void* gfx);
  const unsigned char* icon;
};
