#include <unity.h>
#include "shell.h"

static int enters = 0, exits = 0, ticks = 0;
static bool consume_input = false;
static void f_enter() { enters++; }
static void f_exit() { exits++; }
static void f_tick(uint32_t) { ticks++; }
static bool f_input(Button, BtnEvent) { return consume_input; }
static const App home_app = {"Relogio", f_enter, f_tick, nullptr, f_exit, nullptr};
static const App a1 = {"Toques", f_enter, f_tick, f_input, f_exit, nullptr};
static const App a2 = {"Sobre", f_enter, nullptr, nullptr, f_exit, nullptr};
static const App* apps[] = {&a1, &a2};

static Shell make() {
  enters = exits = ticks = 0; consume_input = false;
  Shell s; s.init(&home_app, apps, 2); return s;
}
void test_starts_at_home_and_enters_home_app() {
  Shell s = make();
  TEST_ASSERT_EQUAL(Shell::HOME, s.screen());
  TEST_ASSERT_EQUAL_PTR(&home_app, s.active());
  TEST_ASSERT_EQUAL_INT(1, enters);
}
void test_ok_opens_launcher_and_c_returns_home() {
  Shell s = make();
  s.input(BTN_OK, EV_PRESS);
  TEST_ASSERT_EQUAL(Shell::LAUNCHER, s.screen());
  TEST_ASSERT_NULL(s.active());
  s.input(BTN_C, EV_PRESS);
  TEST_ASSERT_EQUAL(Shell::HOME, s.screen());
}
void test_cursor_wraps() {
  Shell s = make();
  s.input(BTN_OK, EV_PRESS);
  TEST_ASSERT_EQUAL_UINT8(0, s.cursor());
  s.input(BTN_UP, EV_PRESS);                  // wrap pra cima
  TEST_ASSERT_EQUAL_UINT8(1, s.cursor());
  s.input(BTN_DOWN, EV_PRESS);                // wrap pra baixo
  TEST_ASSERT_EQUAL_UINT8(0, s.cursor());
}
void test_ok_enters_app_and_c_exits_to_launcher() {
  Shell s = make();
  s.input(BTN_OK, EV_PRESS);
  s.input(BTN_DOWN, EV_PRESS);                // cursor → a2
  int before = enters;
  s.input(BTN_OK, EV_PRESS);
  TEST_ASSERT_EQUAL(Shell::APP, s.screen());
  TEST_ASSERT_EQUAL_PTR(&a2, s.active());
  TEST_ASSERT_EQUAL_INT(before + 1, enters);
  s.input(BTN_C, EV_PRESS);
  TEST_ASSERT_EQUAL(Shell::LAUNCHER, s.screen());
  TEST_ASSERT_EQUAL_INT(1, exits);            // a2.on_exit chamado (home não saiu)
}
void test_app_consuming_input_blocks_navigation() {
  Shell s = make();
  s.input(BTN_OK, EV_PRESS);
  s.input(BTN_OK, EV_PRESS);                  // entra em a1
  consume_input = true;
  s.input(BTN_C, EV_PRESS);                   // a1 consome o C
  TEST_ASSERT_EQUAL(Shell::APP, s.screen());
  consume_input = false;
  s.input(BTN_C, EV_PRESS);
  TEST_ASSERT_EQUAL(Shell::LAUNCHER, s.screen());
}
void test_tick_reaches_active_app_only() {
  Shell s = make();
  int t0 = ticks;
  s.tick(100);                                // home tick
  TEST_ASSERT_EQUAL_INT(t0 + 1, ticks);
  s.input(BTN_OK, EV_PRESS);                  // launcher: ninguém ativo
  s.tick(200);
  TEST_ASSERT_EQUAL_INT(t0 + 1, ticks);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_starts_at_home_and_enters_home_app);
  RUN_TEST(test_ok_opens_launcher_and_c_returns_home);
  RUN_TEST(test_cursor_wraps);
  RUN_TEST(test_ok_enters_app_and_c_exits_to_launcher);
  RUN_TEST(test_app_consuming_input_blocks_navigation);
  RUN_TEST(test_tick_reaches_active_app_only);
  return UNITY_END();
}
