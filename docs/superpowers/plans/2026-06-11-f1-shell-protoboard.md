# F1 — Shell na Protoboard: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Shell NokiaOS navegável na protoboard — display 5110 + 4 botões (UP/DOWN/OK/C) + buzzer com ringtone Nokia em RTTTL, apps Relógio/Toques/Sobre. Sem rede.

**Architecture:** Lógica pura (parser RTTTL, debounce, navegação do shell, formatação de hora) vive em `firmware/lib/*` e roda em testes `native` no PC. Glue de hardware (U8g2, LEDC, GPIO) vive em `firmware/src/` e é validada por smoke test no device. Apps são structs com callbacks de lifecycle registrados num shell que gerencia HOME ↔ LAUNCHER ↔ APP.

**Tech Stack:** PlatformIO, Arduino core ESP32 (platform espressif32@^6 → core 2.0.x), U8g2 (PCD8544), Unity (testes native).

**Regras do repo:** repo público e autocontido — código puro, sem dependência de infra específica.

---

## Estrutura de arquivos da F1

```
firmware/
├── platformio.ini
├── include/pins.h               # pinagem v4 (única fonte da verdade)
├── lib/                         # LÓGICA PURA (testável em native, sem Arduino.h)
│   ├── rtttl/{rtttl.h,rtttl.cpp}
│   ├── btnlogic/{btnlogic.h,btnlogic.cpp}
│   ├── clockfmt/{clockfmt.h,clockfmt.cpp}
│   └── nokiacore/{app.h,shell.h,shell.cpp}
├── src/                         # GLUE ARDUINO (validado no device)
│   ├── main.cpp
│   ├── drivers/{buzzer.h,buzzer.cpp,buttons.h,buttons.cpp}
│   ├── ui/{icons.h,statusbar.h,statusbar.cpp,launcher_view.h,launcher_view.cpp}
│   └── apps/{app_clock.h,app_clock.cpp,app_tones.h,app_tones.cpp,app_about.h,app_about.cpp}
└── test/                        # native (pio test -e native)
    ├── test_rtttl/main.cpp
    ├── test_btnlogic/main.cpp
    ├── test_clockfmt/main.cpp
    └── test_shell/main.cpp
```

---

### Task 1: Bootstrap PlatformIO

**Files:**
- Create: `firmware/platformio.ini`, `firmware/include/pins.h`, `firmware/src/main.cpp`, `.gitignore`

- [ ] **Step 1: Criar `firmware/platformio.ini`**

```ini
[env:esp32dev]
platform = espressif32@^6.9.0
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
lib_deps = olikraus/U8g2@^2.36.5

[env:native]
platform = native
test_framework = unity
build_flags = -std=gnu++17
```

- [ ] **Step 2: Criar `firmware/include/pins.h`** (pinagem v4 da spec — única fonte da verdade)

```cpp
#pragma once
// espnokia-phone — pinagem v4 (docs/superpowers/specs/2026-06-10-espnokia-phone-design.md)
#define PIN_LCD_CLK  18  // VSPI SCK
#define PIN_LCD_DIN  23  // VSPI MOSI
#define PIN_LCD_DC   17
#define PIN_LCD_CE    5
#define PIN_LCD_RST  16
#define PIN_LCD_BL    4  // backlight PWM
#define PIN_BTN_UP   19
#define PIN_BTN_DOWN 21
#define PIN_BTN_OK   22
#define PIN_BTN_C    26
#define PIN_BUZZER   25
#define PIN_VBAT     36  // F4 (divisor de tensão)
// F3 (mic I2S): SCK=33, WS=32, SD=35
```

- [ ] **Step 3: Criar `firmware/src/main.cpp` mínimo**

```cpp
#include <Arduino.h>
void setup() { Serial.begin(115200); Serial.println("espnokia-phone boot"); }
void loop() { delay(1000); }
```

- [ ] **Step 4: Criar `.gitignore` na raiz do repo**

```
.pio/
.vscode/
firmware/src/config.h
__pycache__/
.env
```

- [ ] **Step 5: Compilar**

Run: `cd /home/bernardo/projects/espnokia-phone/firmware && pio run -e esp32dev`
Expected: `SUCCESS`

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat(firmware): bootstrap PlatformIO (esp32dev + native)"
```

---

### Task 2: Fiação do display + smoke test visual

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Fiação na protoboard (humano)** — conferir com a tabela:

| 5110 | ESP32 | | 5110 | ESP32 |
|---|---|---|---|---|
| RST | GPIO16 | | CLK | GPIO18 |
| CE | GPIO5 | | VCC | 3V3 |
| DC | GPIO17 | | BL | GPIO4 via R~330Ω |
| DIN | GPIO23 | | GND | GND |

- [ ] **Step 2: Substituir `firmware/src/main.cpp` por smoke do display**

```cpp
#include <Arduino.h>
#include <U8g2lib.h>
#include "pins.h"

U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, PIN_LCD_CE, PIN_LCD_DC, PIN_LCD_RST);

void setup() {
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);
  u8g2.begin();
  u8g2.setContrast(140);  // ajustar 120-160 conforme o módulo
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(8, 20, "espnokia");
  u8g2.drawFrame(0, 0, 84, 48);
  u8g2.sendBuffer();
}
void loop() {
  digitalWrite(PIN_LCD_BL, !digitalRead(PIN_LCD_BL));  // backlight piscando = BL ok
  delay(1000);
}
```

- [ ] **Step 3: Flash + verificação visual**

Run: `pio run -e esp32dev -t upload && pio device monitor`
Expected: tela mostra "espnokia" com moldura; backlight pisca 1Hz. Se tela em branco: ajustar `setContrast`.

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "feat(firmware): smoke test do display 5110 (fiacao validada)"
```

---

### Task 3: Parser RTTTL (TDD, native)

**Files:**
- Create: `firmware/lib/rtttl/rtttl.h`, `firmware/lib/rtttl/rtttl.cpp`
- Test: `firmware/test/test_rtttl/main.cpp`

- [ ] **Step 1: Criar `firmware/lib/rtttl/rtttl.h`**

```cpp
#pragma once
#include <stdint.h>

struct RtttlNote { uint16_t freq_hz; uint16_t dur_ms; };  // freq 0 = pausa

class Rtttl {
 public:
  bool begin(const char* tune);     // parseia header "name:d=4,o=5,b=180:"
  bool next(RtttlNote& out);        // false = acabou
 private:
  const char* p_ = nullptr;
  uint8_t def_dur_ = 4, def_oct_ = 5;
  uint16_t bpm_ = 63;
};
```

- [ ] **Step 2: Escrever testes `firmware/test/test_rtttl/main.cpp`**

```cpp
#include <unity.h>
#include "rtttl.h"

void test_header_and_quarter_note() {
  Rtttl r; RtttlNote n;
  TEST_ASSERT_TRUE(r.begin("x:d=4,o=5,b=120:a"));
  TEST_ASSERT_TRUE(r.next(n));
  TEST_ASSERT_EQUAL_UINT16(880, n.freq_hz);   // a5: RTTTL o4 tem A=440 → a5 = 880Hz
  TEST_ASSERT_EQUAL_UINT16(500, n.dur_ms);    // semínima @120bpm = 500ms
  TEST_ASSERT_FALSE(r.next(n));
}
void test_explicit_duration_and_octave() {
  Rtttl r; RtttlNote n;
  r.begin("x:d=4,o=5,b=120:8e6");
  r.next(n);
  TEST_ASSERT_EQUAL_UINT16(1320, n.freq_hz);  // e6 = 330<<2 (E6 real 1318.5)
  TEST_ASSERT_EQUAL_UINT16(250, n.dur_ms);    // colcheia @120bpm
}
void test_sharp_pause_dotted() {
  Rtttl r; RtttlNote n;
  r.begin("x:d=8,o=5,b=120:f#,p,4c.");
  r.next(n); TEST_ASSERT_EQUAL_UINT16(740, n.freq_hz);   // f#5 = 370<<1
  r.next(n); TEST_ASSERT_EQUAL_UINT16(0, n.freq_hz);     // pausa
  r.next(n); TEST_ASSERT_EQUAL_UINT16(750, n.dur_ms);    // 4c. = 500*1.5
}
void test_nokia_tune_parses_all_13_notes() {
  Rtttl r; RtttlNote n; int count = 0;
  r.begin("Nokia:d=4,o=5,b=225:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a");
  while (r.next(n)) count++;
  TEST_ASSERT_EQUAL_INT(13, count);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_header_and_quarter_note);
  RUN_TEST(test_explicit_duration_and_octave);
  RUN_TEST(test_sharp_pause_dotted);
  RUN_TEST(test_nokia_tune_parses_all_13_notes);
  return UNITY_END();
}
```

- [ ] **Step 3: Rodar e ver falhar**

Run: `pio test -e native -f test_rtttl`
Expected: FAIL (linker: `Rtttl::begin` indefinido)

- [ ] **Step 4: Implementar `firmware/lib/rtttl/rtttl.cpp`**

```cpp
#include "rtttl.h"
#include <string.h>
#include <ctype.h>

// semitom 0=c ... 11=b, oitava 4; oitavas maiores = << (o-4)
static const uint16_t kOct4[12] = {262,277,294,311,330,349,370,392,415,440,466,494};
static const int8_t kIdx[7] = {9,11,0,2,4,5,7};  // a b c d e f g

static uint16_t read_num(const char*& p) {
  uint16_t v = 0;
  while (isdigit((unsigned char)*p)) v = v * 10 + (*p++ - '0');
  return v;
}

bool Rtttl::begin(const char* tune) {
  const char* p = strchr(tune, ':');
  if (!p) return false;
  p++;  // seção de defaults
  while (*p && *p != ':') {
    if (*p == 'd') { p += 2; def_dur_ = (uint8_t)read_num(p); }
    else if (*p == 'o') { p += 2; def_oct_ = (uint8_t)read_num(p); }
    else if (*p == 'b') { p += 2; bpm_ = read_num(p); }
    else p++;
  }
  if (*p != ':') return false;
  p_ = p + 1;
  return true;
}

bool Rtttl::next(RtttlNote& out) {
  if (!p_ || !*p_) return false;
  while (*p_ == ',' || *p_ == ' ') p_++;
  if (!*p_) return false;

  uint16_t dur = read_num(p_);
  if (dur == 0) dur = def_dur_;

  char c = (char)tolower((unsigned char)*p_);
  int8_t semi = -1;
  if (c >= 'a' && c <= 'g') { semi = kIdx[c - 'a']; p_++; }
  else if (c == 'p') { p_++; }
  else return false;
  if (*p_ == '#') { semi++; p_++; }

  bool dotted = false;
  if (*p_ == '.') { dotted = true; p_++; }       // ponto antes ou depois da oitava
  uint16_t oct = read_num(p_);
  if (oct == 0) oct = def_oct_;
  if (*p_ == '.') { dotted = true; p_++; }

  uint32_t ms = (60000UL * 4 / bpm_) / dur;
  if (dotted) ms += ms / 2;

  out.freq_hz = (semi < 0) ? 0 : (uint16_t)(kOct4[semi] << (oct - 4));
  out.dur_ms = (uint16_t)ms;
  return true;
}
```

- [ ] **Step 5: Rodar e ver passar**

Run: `pio test -e native -f test_rtttl`
Expected: `4 Tests 0 Failures`

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat(firmware): parser RTTTL puro com testes native"
```

---

### Task 4: Driver do buzzer (glue LEDC) + fiação + tune no boot

**Files:**
- Create: `firmware/src/drivers/buzzer.h`, `firmware/src/drivers/buzzer.cpp`
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Fiação (humano):** buzzer passivo: + no GPIO25, − no GND.

- [ ] **Step 2: Criar `firmware/src/drivers/buzzer.h`**

```cpp
#pragma once
#include <stdint.h>

namespace buzzer {
void init();
void beep(uint16_t freq_hz, uint16_t dur_ms);  // bip único (tecla)
void play(const char* rtttl);                  // toca tune, não-bloqueante
void stop();
bool busy();
void tick(uint32_t now_ms);                    // chamar todo loop()
}
```

- [ ] **Step 3: Criar `firmware/src/drivers/buzzer.cpp`**

```cpp
#include "buzzer.h"
#include <Arduino.h>
#include "pins.h"
#include "rtttl.h"

namespace buzzer {
static const uint8_t kCh = 0;
static Rtttl rtttl_;
static bool playing_ = false;       // tune em andamento
static uint32_t note_end_ = 0;      // fim da nota/bip atual
static bool tone_on_ = false;

void init() {
  ledcSetup(kCh, 2000, 10);
  ledcAttachPin(PIN_BUZZER, kCh);
  ledcWriteTone(kCh, 0);
}
static void tone_for(uint16_t f, uint16_t ms, uint32_t now) {
  ledcWriteTone(kCh, f);
  tone_on_ = true;
  note_end_ = now + ms;
}
void beep(uint16_t f, uint16_t ms) { playing_ = false; tone_for(f, ms, millis()); }
void play(const char* tune) { playing_ = rtttl_.begin(tune); if (playing_) note_end_ = 0; }
void stop() { playing_ = false; tone_on_ = false; ledcWriteTone(kCh, 0); }
bool busy() { return playing_ || tone_on_; }

void tick(uint32_t now) {
  if (tone_on_ && now >= note_end_) { ledcWriteTone(kCh, 0); tone_on_ = false; }
  if (playing_ && now >= note_end_) {
    RtttlNote n;
    if (rtttl_.next(n)) {
      tone_for(n.freq_hz, (uint16_t)(n.dur_ms * 9 / 10), now);  // 10% gap entre notas
      note_end_ = now + n.dur_ms;
      playing_ = true;
    } else playing_ = false;
  }
}
}  // namespace buzzer
```

- [ ] **Step 4: Tocar o Nokia tune no boot — editar `firmware/src/main.cpp`** (adicionar ao smoke da Task 2)

```cpp
// no topo:
#include "drivers/buzzer.h"
static const char* kNokiaTune =
    "Nokia:d=4,o=5,b=225:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a";
// no fim do setup():
buzzer::init();
buzzer::play(kNokiaTune);
// no loop(), substituir o delay(1000) por:
buzzer::tick(millis());
delay(2);
```

(O pisca de backlight sai do loop — manter `digitalWrite(PIN_LCD_BL, HIGH)` fixo no setup.)

- [ ] **Step 5: Flash + verificação auditiva**

Run: `pio run -e esp32dev -t upload`
Expected: o ringtone Nokia clássico toca no boot. 🎵

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat(firmware): driver buzzer LEDC nao-bloqueante + nokia tune no boot"
```

---

### Task 5: Debounce de botões (TDD) + glue + fiação

**Files:**
- Create: `firmware/lib/btnlogic/btnlogic.h`, `firmware/lib/btnlogic/btnlogic.cpp`
- Create: `firmware/src/drivers/buttons.h`, `firmware/src/drivers/buttons.cpp`
- Test: `firmware/test/test_btnlogic/main.cpp`

- [ ] **Step 1: Criar `firmware/lib/btnlogic/btnlogic.h`**

```cpp
#pragma once
#include <stdint.h>

enum Button : uint8_t { BTN_UP = 0, BTN_DOWN, BTN_OK, BTN_C, BTN_COUNT };
enum BtnEvent : int8_t { EV_NONE = -1, EV_PRESS = 0, EV_RELEASE = 1 };

struct BtnState {
  bool stable = false;     // estado debounced (true = pressionado)
  bool last_raw = false;
  uint32_t t_change = 0;
};

// Alimentar com leitura crua a cada loop; retorna evento debounced.
BtnEvent btn_step(BtnState& s, bool raw_pressed, uint32_t now_ms,
                  uint32_t debounce_ms = 25);
```

- [ ] **Step 2: Escrever testes `firmware/test/test_btnlogic/main.cpp`**

```cpp
#include <unity.h>
#include "btnlogic.h"

void test_press_after_debounce() {
  BtnState s;
  TEST_ASSERT_EQUAL_INT(EV_NONE, btn_step(s, true, 0));     // borda crua
  TEST_ASSERT_EQUAL_INT(EV_NONE, btn_step(s, true, 10));    // ainda instável
  TEST_ASSERT_EQUAL_INT(EV_PRESS, btn_step(s, true, 30));   // estável >25ms
  TEST_ASSERT_EQUAL_INT(EV_NONE, btn_step(s, true, 40));    // sem repetição
}
void test_bounce_is_ignored() {
  BtnState s;
  btn_step(s, true, 0);
  btn_step(s, false, 5);    // bounce reseta o timer
  btn_step(s, true, 12);
  TEST_ASSERT_EQUAL_INT(EV_NONE, btn_step(s, true, 30));    // 30-12 < 25
  TEST_ASSERT_EQUAL_INT(EV_PRESS, btn_step(s, true, 40));
}
void test_release_event() {
  BtnState s;
  btn_step(s, true, 0);
  btn_step(s, true, 30);                                     // PRESS
  btn_step(s, false, 100);
  TEST_ASSERT_EQUAL_INT(EV_RELEASE, btn_step(s, false, 130));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_press_after_debounce);
  RUN_TEST(test_bounce_is_ignored);
  RUN_TEST(test_release_event);
  return UNITY_END();
}
```

- [ ] **Step 3: Rodar e ver falhar**

Run: `pio test -e native -f test_btnlogic`
Expected: FAIL (`btn_step` indefinido)

- [ ] **Step 4: Implementar `firmware/lib/btnlogic/btnlogic.cpp`**

```cpp
#include "btnlogic.h"

BtnEvent btn_step(BtnState& s, bool raw, uint32_t now, uint32_t debounce_ms) {
  if (raw != s.last_raw) { s.last_raw = raw; s.t_change = now; }
  if (raw != s.stable && (now - s.t_change) >= debounce_ms) {
    s.stable = raw;
    return raw ? EV_PRESS : EV_RELEASE;
  }
  return EV_NONE;
}
```

- [ ] **Step 5: Rodar e ver passar**

Run: `pio test -e native -f test_btnlogic`
Expected: `3 Tests 0 Failures`

- [ ] **Step 6: Criar glue `firmware/src/drivers/buttons.h`**

```cpp
#pragma once
#include "btnlogic.h"

namespace buttons {
void init();
// Varre os 4 botões; retorna via parâmetros o primeiro evento do ciclo.
bool poll(uint32_t now_ms, Button& btn_out, BtnEvent& ev_out);
}
```

- [ ] **Step 7: Criar `firmware/src/drivers/buttons.cpp`**

```cpp
#include "buttons.h"
#include <Arduino.h>
#include "pins.h"

namespace buttons {
static const uint8_t kPins[BTN_COUNT] = {PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_OK, PIN_BTN_C};
static BtnState states[BTN_COUNT];

void init() {
  for (uint8_t i = 0; i < BTN_COUNT; i++) pinMode(kPins[i], INPUT_PULLUP);
}
bool poll(uint32_t now, Button& btn_out, BtnEvent& ev_out) {
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    bool raw = digitalRead(kPins[i]) == LOW;  // ativo-baixo
    BtnEvent ev = btn_step(states[i], raw, now);
    if (ev != EV_NONE) { btn_out = (Button)i; ev_out = ev; return true; }
  }
  return false;
}
}  // namespace buttons
```

- [ ] **Step 8: Fiação (humano):** 4 botões, uma perna no GPIO (19=UP, 21=DOWN, 22=OK, 26=C), perna diagonal no GND. Sem resistor (pull-up interno).

- [ ] **Step 9: Smoke no device — editar `main.cpp`** (adicionar no `loop()`, antes do `buzzer::tick`)

```cpp
// no topo: #include "drivers/buttons.h"
// no setup(): buttons::init();
// no loop():
Button b; BtnEvent e;
if (buttons::poll(millis(), b, e) && e == EV_PRESS) {
  static const char* kNames[] = {"UP", "DOWN", "OK", "C"};
  Serial.println(kNames[b]);
  buzzer::beep(1250, 40);  // keypad click
}
```

Run: `pio run -e esp32dev -t upload && pio device monitor`
Expected: cada botão imprime o nome certo e dá um bip. Conferir os 4.

- [ ] **Step 10: Commit**

```bash
git add -A && git commit -m "feat(firmware): debounce testado + driver de 4 botoes com keypad beep"
```

---

### Task 6: Núcleo do shell (TDD, native)

**Files:**
- Create: `firmware/lib/nokiacore/app.h`, `firmware/lib/nokiacore/shell.h`, `firmware/lib/nokiacore/shell.cpp`
- Test: `firmware/test/test_shell/main.cpp`

- [ ] **Step 1: Criar `firmware/lib/nokiacore/app.h`**

```cpp
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
```

- [ ] **Step 2: Criar `firmware/lib/nokiacore/shell.h`**

```cpp
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
```

- [ ] **Step 3: Escrever testes `firmware/test/test_shell/main.cpp`**

```cpp
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
```

- [ ] **Step 4: Rodar e ver falhar**

Run: `pio test -e native -f test_shell`
Expected: FAIL (Shell indefinido)

- [ ] **Step 5: Implementar `firmware/lib/nokiacore/shell.cpp`**

```cpp
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
```

- [ ] **Step 6: Rodar e ver passar**

Run: `pio test -e native -f test_shell`
Expected: `6 Tests 0 Failures`

- [ ] **Step 7: Commit**

```bash
git add -A && git commit -m "feat(firmware): nucleo do shell (home/launcher/app) com testes"
```

---

### Task 7: Formatação do relógio (TDD, native)

**Files:**
- Create: `firmware/lib/clockfmt/clockfmt.h`, `firmware/lib/clockfmt/clockfmt.cpp`
- Test: `firmware/test/test_clockfmt/main.cpp`

- [ ] **Step 1: Criar `firmware/lib/clockfmt/clockfmt.h`**

```cpp
#pragma once
#include <stdint.h>

// F1 não tem rede: relógio "anda" a partir de 12:00 no boot (estilo
// videocassete). O ':' pisca a cada segundo, como nos Nokia reais.
// out deve ter >= 6 bytes ("HH:MM\0"); colon_on diz se o ':' aparece.
void clock_format(uint32_t ms_since_boot, char out[6], bool* colon_on);
```

- [ ] **Step 2: Escrever testes `firmware/test/test_clockfmt/main.cpp`**

```cpp
#include <unity.h>
#include <string.h>
#include "clockfmt.h"

void test_boot_shows_noon() {
  char s[6]; bool c;
  clock_format(0, s, &c);
  TEST_ASSERT_EQUAL_STRING("12:00", s);
  TEST_ASSERT_TRUE(c);
}
void test_minutes_and_hours_advance() {
  char s[6]; bool c;
  clock_format(61UL * 60 * 1000, s, &c);      // +1h01
  TEST_ASSERT_EQUAL_STRING("13:01", s);
}
void test_wraps_past_midnight() {
  char s[6]; bool c;
  clock_format((12UL * 60 + 1) * 60 * 1000, s, &c);  // 12:00 + 12h01
  TEST_ASSERT_EQUAL_STRING("00:01", s);
}
void test_colon_blinks_each_second() {
  char s[6]; bool c;
  clock_format(1000, s, &c);
  TEST_ASSERT_FALSE(c);                       // segundo ímpar: apagado
  clock_format(2000, s, &c);
  TEST_ASSERT_TRUE(c);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_shows_noon);
  RUN_TEST(test_minutes_and_hours_advance);
  RUN_TEST(test_wraps_past_midnight);
  RUN_TEST(test_colon_blinks_each_second);
  return UNITY_END();
}
```

- [ ] **Step 3: Rodar e ver falhar**

Run: `pio test -e native -f test_clockfmt`
Expected: FAIL

- [ ] **Step 4: Implementar `firmware/lib/clockfmt/clockfmt.cpp`**

```cpp
#include "clockfmt.h"

void clock_format(uint32_t ms, char out[6], bool* colon_on) {
  uint32_t total_min = ms / 60000UL;
  uint32_t h = (12 + total_min / 60) % 24;
  uint32_t m = total_min % 60;
  out[0] = (char)('0' + h / 10);
  out[1] = (char)('0' + h % 10);
  out[2] = ':';
  out[3] = (char)('0' + m / 10);
  out[4] = (char)('0' + m % 10);
  out[5] = '\0';
  *colon_on = ((ms / 1000) % 2) == 0;
}
```

- [ ] **Step 5: Rodar e ver passar**

Run: `pio test -e native -f test_clockfmt`
Expected: `4 Tests 0 Failures`

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat(firmware): formatacao do relogio com colon piscante"
```

---

### Task 8: UI — ícones, status bar e view do launcher

**Files:**
- Create: `firmware/src/ui/icons.h`, `firmware/src/ui/statusbar.h`, `firmware/src/ui/statusbar.cpp`, `firmware/src/ui/launcher_view.h`, `firmware/src/ui/launcher_view.cpp`

- [ ] **Step 1: Criar `firmware/src/ui/icons.h`** (XBM 8×6, estética pixel)

```cpp
#pragma once
// Antena WiFi sem barras (F1 está offline) — mastro + 0 barras
static const unsigned char kIconWifiOff[] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x07};
// Antena com 3 barras (usada a partir da F2; já fica pronta)
static const unsigned char kIconWifiOn[] = {0x21, 0x21, 0x29, 0x29, 0x2d, 0x2f};
// Bateria cheia (VBAT real só na F4)
static const unsigned char kIconBatt[] = {0x1e, 0x3f, 0x3f, 0x3f, 0x3f, 0x1e};
```

- [ ] **Step 2: Criar `firmware/src/ui/statusbar.h`**

```cpp
#pragma once
#include <U8g2lib.h>

// Barra de 8px no topo: [wifi] ... hora ... [bateria]
void statusbar_draw(U8G2& g, const char* clock_str, bool wifi_on);
```

- [ ] **Step 3: Criar `firmware/src/ui/statusbar.cpp`**

```cpp
#include "statusbar.h"
#include "icons.h"

void statusbar_draw(U8G2& g, const char* clock_str, bool wifi_on) {
  g.setFont(u8g2_font_4x6_tr);
  g.drawXBM(1, 1, 6, 6, wifi_on ? kIconWifiOn : kIconWifiOff);
  g.drawStr(42 - (int)g.getStrWidth(clock_str) / 2, 7, clock_str);
  g.drawXBM(77, 1, 6, 6, kIconBatt);
  g.drawHLine(0, 8, 84);
}
```

- [ ] **Step 4: Criar `firmware/src/ui/launcher_view.h`**

```cpp
#pragma once
#include <U8g2lib.h>
#include "shell.h"

// Lista vertical estilo 3310: até 4 itens, selecionado em barra invertida.
void launcher_view_draw(U8G2& g, const Shell& shell);
```

- [ ] **Step 5: Criar `firmware/src/ui/launcher_view.cpp`**

```cpp
#include "launcher_view.h"

void launcher_view_draw(U8G2& g, const Shell& shell) {
  g.setFont(u8g2_font_profont11_tr);
  const uint8_t kRow = 10, kTop = 10;
  uint8_t first = (shell.cursor() / 4) * 4;  // paginação de 4 em 4
  for (uint8_t i = 0; i < 4 && first + i < shell.app_count(); i++) {
    uint8_t idx = first + i;
    int y = kTop + i * kRow;
    if (idx == shell.cursor()) {
      g.drawBox(0, y, 84, kRow);
      g.setDrawColor(0);
      g.drawStr(3, y + 8, shell.app_at(idx)->name);
      g.setDrawColor(1);
    } else {
      g.drawStr(3, y + 8, shell.app_at(idx)->name);
    }
  }
}
```

- [ ] **Step 6: Compilar (sem flash ainda — integração visual vem na Task 10)**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`

- [ ] **Step 7: Commit**

```bash
git add -A && git commit -m "feat(firmware): status bar, icones e view do launcher"
```

---

### Task 9: Apps — Relógio, Toques, Sobre

**Files:**
- Create: `firmware/src/apps/app_clock.h`, `firmware/src/apps/app_clock.cpp`
- Create: `firmware/src/apps/app_tones.h`, `firmware/src/apps/app_tones.cpp`
- Create: `firmware/src/apps/app_about.h`, `firmware/src/apps/app_about.cpp`

- [ ] **Step 1: Criar `firmware/src/apps/app_clock.h`**

```cpp
#pragma once
#include "app.h"
extern const App app_clock;  // app HOME
```

- [ ] **Step 2: Criar `firmware/src/apps/app_clock.cpp`**

```cpp
#include "app_clock.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "clockfmt.h"

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char hhmm[6]; bool colon;
  clock_format(millis(), hhmm, &colon);
  if (!colon) hhmm[2] = ' ';
  g.setFont(u8g2_font_profont22_tn);
  g.drawStr(42 - (int)g.getStrWidth(hhmm) / 2, 32, hhmm);
  g.setFont(u8g2_font_4x6_tr);
  g.drawStr(42 - (int)g.getStrWidth("espnokia") / 2, 45, "espnokia");
}
const App app_clock = {"Relogio", nullptr, nullptr, nullptr, nullptr, render};
```

- [ ] **Step 3: Criar `firmware/src/apps/app_tones.h`**

```cpp
#pragma once
#include "app.h"
extern const App app_tones;
```

- [ ] **Step 4: Criar `firmware/src/apps/app_tones.cpp`**

```cpp
#include "app_tones.h"
#include <U8g2lib.h>
#include "drivers/buzzer.h"

struct Tone { const char* name; const char* rtttl; };
static const Tone kTones[] = {
    {"Nokia", "Nokia:d=4,o=5,b=225:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a"},
    {"Kuckuck", "Kuckuck:d=4,o=5,b=125:8g,8e,8g,8e,8g,8e,8c6,8a,8g,8e,8f,8d,2c"},
    {"Sweep", "Sweep:d=16,o=5,b=140:c,e,g,c6,g,e,c"},
};
static const uint8_t kCount = sizeof(kTones) / sizeof(kTones[0]);
static uint8_t cur = 0;

static bool input(Button b, BtnEvent e) {
  if (e != EV_PRESS) return false;
  if (b == BTN_UP) { cur = (cur + kCount - 1) % kCount; return true; }
  if (b == BTN_DOWN) { cur = (cur + 1) % kCount; return true; }
  if (b == BTN_OK) { buzzer::play(kTones[cur].rtttl); return true; }
  return false;  // C não consumido → shell volta pro launcher
}
static void on_exit() { buzzer::stop(); }
static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_profont11_tr);
  g.drawStr(3, 18, "Toque:");
  g.drawBox(0, 22, 84, 11);
  g.setDrawColor(0);
  g.drawStr(6, 31, kTones[cur].name);
  g.setDrawColor(1);
  g.setFont(u8g2_font_4x6_tr);
  g.drawStr(3, 45, buzzer::busy() ? "tocando..." : "OK toca  C volta");
}
const App app_tones = {"Toques", nullptr, nullptr, input, on_exit, render};
```

- [ ] **Step 5: Criar `firmware/src/apps/app_about.h`**

```cpp
#pragma once
#include "app.h"
extern const App app_about;
```

- [ ] **Step 6: Criar `firmware/src/apps/app_about.cpp`**

```cpp
#include "app_about.h"
#include <Arduino.h>
#include <U8g2lib.h>

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  char buf[24];
  g.setFont(u8g2_font_profont11_tr);
  g.drawStr(3, 18, "espnokia-phone");
  g.setFont(u8g2_font_4x6_tr);
  g.drawStr(3, 28, "fw F1 0.1.0");
  snprintf(buf, sizeof(buf), "heap %u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  g.drawStr(3, 36, buf);
  snprintf(buf, sizeof(buf), "up %lu s", (unsigned long)(millis() / 1000));
  g.drawStr(3, 44, buf);
}
const App app_about = {"Sobre", nullptr, nullptr, nullptr, nullptr, render};
```

- [ ] **Step 7: Compilar**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`

- [ ] **Step 8: Commit**

```bash
git add -A && git commit -m "feat(firmware): apps relogio, toques e sobre"
```

---

### Task 10: Integração final no main.cpp (boot splash + loop do OS)

**Files:**
- Modify: `firmware/src/main.cpp` (substituir todo o conteúdo)

- [ ] **Step 1: Reescrever `firmware/src/main.cpp`**

```cpp
#include <Arduino.h>
#include <U8g2lib.h>
#include "pins.h"
#include "shell.h"
#include "clockfmt.h"
#include "drivers/buzzer.h"
#include "drivers/buttons.h"
#include "ui/statusbar.h"
#include "ui/launcher_view.h"
#include "apps/app_clock.h"
#include "apps/app_tones.h"
#include "apps/app_about.h"

static U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, PIN_LCD_CE, PIN_LCD_DC, PIN_LCD_RST);
static Shell shell;
static const App* kApps[] = {&app_tones, &app_about};
static const char* kNokiaTune =
    "Nokia:d=4,o=5,b=225:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a";

static void splash() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(42 - (int)u8g2.getStrWidth("espnokia") / 2, 22, "espnokia");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(42 - (int)u8g2.getStrWidth("phone") / 2, 32, "phone");
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);
  u8g2.begin();
  u8g2.setContrast(140);
  buzzer::init();
  buttons::init();
  splash();
  buzzer::play(kNokiaTune);          // não-bloqueante: splash fica até o fim
  uint32_t t0 = millis();
  while (buzzer::busy() && millis() - t0 < 4000) { buzzer::tick(millis()); delay(2); }
  shell.init(&app_clock, kApps, sizeof(kApps) / sizeof(kApps[0]));
}

void loop() {
  uint32_t now = millis();

  Button b; BtnEvent e;
  if (buttons::poll(now, b, e)) {
    if (e == EV_PRESS) buzzer::beep(1250, 30);  // keypad click
    shell.input(b, e);
  }
  shell.tick(now);
  buzzer::tick(now);

  static uint32_t last_render = 0;
  if (now - last_render >= 50) {                // ~20 fps
    last_render = now;
    char hhmm[6]; bool colon;
    clock_format(now, hhmm, &colon);
    if (!colon) hhmm[2] = ' ';

    u8g2.clearBuffer();
    statusbar_draw(u8g2, hhmm, false);          // wifi off na F1
    if (shell.screen() == Shell::LAUNCHER) launcher_view_draw(u8g2, shell);
    else if (shell.active() && shell.active()->on_render)
      shell.active()->on_render(&u8g2);
    u8g2.sendBuffer();
  }
  delay(2);
}
```

- [ ] **Step 2: Compilar e flashar**

Run: `pio run -e esp32dev -t upload`
Expected: `SUCCESS`

- [ ] **Step 3: Smoke test completo no device (checklist humano)**

1. Boot: splash "espnokia phone" + Nokia tune tocando. ✓
2. Home: relógio grande "12:00" com `:` piscando + status bar (antena vazia, hora, bateria). ✓
3. OK → launcher com "Toques"/"Sobre", barra invertida no selecionado. ✓
4. UP/DOWN navega com wrap; cada tecla dá um click. ✓
5. OK em "Toques" → escolher toque, OK toca, C volta. ✓
6. "Sobre" mostra heap/uptime subindo. ✓
7. C no launcher → volta pra home. ✓

- [ ] **Step 4: Rodar todos os testes native (regressão final)**

Run: `pio test -e native`
Expected: `test_rtttl` + `test_btnlogic` + `test_clockfmt` + `test_shell` → 17 testes, 0 falhas

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat(firmware): F1 completa — shell navegavel com clock, toques e sobre"
```

---

### Task 11: README mínimo + push

**Files:**
- Create: `README.md` (raiz do repo)

- [ ] **Step 1: Criar `README.md`** (mínimo honesto — o caprichado vem na F4)

```markdown
# espnokia-phone

Um "Nokia OS" fake e carinhoso num ESP32 + display Nokia 5110 — estética de
2000, recursos de 2026 (em construção: app da Copa 2026 e conversa por voz
com Claude).

**Status:** F1 — shell navegável (relógio, toques RTTTL, sobre). 🚧

## Hardware

ESP32 WROOM DevKit · Nokia 5110 (PCD8544) · 4 botões (UP/DOWN/OK/C) ·
buzzer passivo · (em breve: mic I2S INMP441, LiPo)

Pinagem: `firmware/include/pins.h` · Spec completa: `docs/superpowers/specs/`

## Build

```bash
cd firmware
pio run -e esp32dev -t upload   # flash
pio test -e native              # testes (não precisam de hardware)
```
```

- [ ] **Step 2: Commit e push**

```bash
git add -A && git commit -m "docs: README minimo da F1"
git push origin master
```

---

## Self-review (feito)

- **Cobertura da spec (F1):** display+botões+buzzer ✓, shell navegável ✓, app_clock ✓, RTTTL com ringtone Nokia ✓, sem rede ✓ (statusbar já recebe `wifi_on` pra F2). Apps Toques/Sobre são extra mínimo pro launcher ter conteúdo real.
- **Placeholders:** nenhum — todo step tem código/comando completo.
- **Consistência de tipos:** `Button`/`BtnEvent` definidos uma vez em `btnlogic.h` e usados por shell/buttons/apps; `App.on_render(void*)` recebe `U8G2*` no glue em todos os apps; pinos só via `pins.h`.
```
