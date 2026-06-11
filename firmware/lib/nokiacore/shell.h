#pragma once
#include "app.h"

// HOME (app home fixo) ↔ LAUNCHER (lista) ↔ APP (app da lista).
// OK na home abre o launcher; OK no launcher entra no app sob o cursor;
// C volta (APP→LAUNCHER→HOME). UP/DOWN movem o cursor com wrap.
class Shell {
 public:
  enum Screen : uint8_t { HOME, LAUNCHER, APP };

  void init(const App* home, const App** apps, uint8_t count);
  void input(Button b, BtnEvent e);
  void tick(uint32_t now_ms);

  Screen screen() const { return screen_; }
  uint8_t cursor() const { return cursor_; }
  uint8_t app_count() const { return count_; }
  const App* app_at(uint8_t i) const { return apps_[i]; }
  const App* active() const;  // home em HOME; app aberto em APP; nullptr em LAUNCHER

 private:
  void enter(const App* a);
  void exit_active();
  const App* home_ = nullptr;
  const App** apps_ = nullptr;
  uint8_t count_ = 0, cursor_ = 0;
  Screen screen_ = HOME;
  const App* open_ = nullptr;
};
