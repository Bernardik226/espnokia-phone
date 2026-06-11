#pragma once
#include <stdint.h>
#include "btnlogic.h"

// Contrato de app do NokiaOS. on_render recebe void* para manter o core puro
// (o glue passa U8G2*). Callbacks podem ser nullptr.
struct App {
  const char* name;
  void (*on_enter)();
  void (*on_tick)(uint32_t now_ms);
  bool (*on_input)(Button b, BtnEvent e);  // true = consumiu o evento
  void (*on_exit)();
  void (*on_render)(void* gfx);
};
