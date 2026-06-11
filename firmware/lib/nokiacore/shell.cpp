#include "shell.h"

void Shell::init(const App* home, const App** apps, uint8_t count) {
  home_ = home; apps_ = apps; count_ = count;
  screen_ = HOME; cursor_ = 0; open_ = nullptr;
  enter(home_);
}
const App* Shell::active() const {
  if (screen_ == HOME) return home_;
  if (screen_ == APP) return open_;
  return nullptr;
}
void Shell::enter(const App* a) { if (a && a->on_enter) a->on_enter(); }
void Shell::exit_active() {
  const App* a = active();
  if (a && a->on_exit) a->on_exit();
}
void Shell::input(Button b, BtnEvent e) {
  const App* a = active();
  if (a && a->on_input && a->on_input(b, e)) return;  // app consumiu
  if (e != EV_PRESS) return;                          // navegação só em PRESS

  switch (screen_) {
    case HOME:
      if (b == BTN_OK) { screen_ = LAUNCHER; cursor_ = 0; }
      break;
    case LAUNCHER:
      if (b == BTN_UP) cursor_ = (cursor_ + count_ - 1) % count_;
      else if (b == BTN_DOWN) cursor_ = (cursor_ + 1) % count_;
      else if (b == BTN_OK) { open_ = apps_[cursor_]; screen_ = APP; enter(open_); }
      else if (b == BTN_C) screen_ = HOME;
      break;
    case APP:
      if (b == BTN_C) { exit_active(); open_ = nullptr; screen_ = LAUNCHER; }
      break;
  }
}
void Shell::tick(uint32_t now) {
  const App* a = active();
  if (a && a->on_tick) a->on_tick(now);
}
