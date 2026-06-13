<h3 align="center">
  <a href="../README.md">📟 Back to the project</a>&nbsp;&nbsp;·&nbsp;&nbsp;🧩 Make your own app
</h3>

<p align="center">
  🇬🇧 English&nbsp;&nbsp;·&nbsp;&nbsp;<a href="MAKE_AN_APP.pt-br.md">🇧🇷 Português</a>
</p>

# Make your own app

> This is a **pointer, not a rulebook** — a suggested path for adding your own
> screen to NokiaOS. The shell is small on purpose; once you've seen one app,
> you can bend the rest to taste.

NokiaOS is an **app shell**. Each app is a plain struct of callbacks the shell
calls at the right moments — there's no inheritance, no framework. Adding one is
three small steps: **write the app, give it a name, register it.**

## 1. The contract

Every app fills in [`firmware/lib/nokiacore/app.h`](../firmware/lib/nokiacore/app.h).
Any callback you don't need can be `nullptr`:

```cpp
struct App {
  uint8_t name_id;                         // its name in the launcher (an i18n StrId)
  void (*on_enter)();                      // opened — set up state
  void (*on_tick)(uint32_t now_ms);        // called every loop — animate, poll
  bool (*on_input)(Button b, BtnEvent e);  // a key event — return true if you consumed it
  void (*on_exit)();                       // closed — tear down
  void (*on_render)(void* gfx);            // draw a frame (cast to U8G2*)
  const unsigned char* icon;               // 28×24 XBM for the menu (or nullptr)
  const unsigned char* const* anim;        // 28×24 frames ending in nullptr (or nullptr)
};
```

`on_render` takes a `void*` so the core stays hardware-agnostic; the glue passes
a `U8G2*`. Cast it once and draw.

## 2. A minimal app

Create `firmware/src/apps/app_hello.h`:

```cpp
#pragma once
#include "app.h"
extern const App app_hello;
```

…and `firmware/src/apps/app_hello.cpp`:

```cpp
#include "app_hello.h"
#include <U8g2lib.h>
#include "i18n.h"
#include "ui/fonts3310.h"
#include "ui/nokia_ui.h"

static uint8_t taps_ = 0;

static void on_enter() { taps_ = 0; }

static bool on_input(Button b, BtnEvent e) {
  if (b == Button::OK && e == BtnEvent::PRESS) { taps_++; return true; }
  return false;   // let the shell handle C (back), UP/DOWN, etc.
}

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_3310_small);
  nokia_ui::text_bold_center(g, 8, tr(STR_APP_HELLO));
  char buf[16];
  snprintf(buf, sizeof(buf), "taps: %u", taps_);
  g.drawUTF8(2, 28, buf);
  nokia_ui::softkey(g, tr(STR_BACK));
}

const App app_hello = {STR_APP_HELLO, on_enter, nullptr, on_input,
                       nullptr, render, nullptr, nullptr};
```

Helpers worth knowing (see [`nokia_ui.h`](../firmware/src/ui/nokia_ui.h)):
`text_bold_center`, `text_bold`, `softkey`. For accented text use `drawUTF8`
(`drawStr` stops at ASCII 127). The body font is `u8g2_font_3310_small`.

## 3. Name it (i18n)

The launcher shows `name_id`, a string id from the i18n table in
[`firmware/lib/i18n`](../firmware/lib/i18n). Add `STR_APP_HELLO` (and any other
strings the app shows) there, with a translation per language — `tr(id)` returns
the right one at runtime. Keeping user-facing text in i18n is what keeps the
device in 9 languages for free.

## 4. Register it

In [`firmware/src/main.cpp`](../firmware/src/main.cpp), include the header and
drop the app into the launcher list:

```cpp
#include "apps/app_hello.h"

static const App* kApps[] = {&app_clock,   &app_claude, &app_esportes,
                             &app_weather, &app_tones,  &app_hello,
                             &app_settings};
```

That's it — build, flash, and your app is in the menu.

## 5. Optional: an icon &amp; selection animation

Menu icons are **28×24 bitmaps written as text** in
[`firmware/assets/assets.txt`](../firmware/assets/assets.txt) (a tiny grid DSL —
`#` is a lit pixel) and compiled to XBM by the tool in [`tools/`](../tools):

```bash
python3 tools/grid2xbm.py firmware/assets/assets.txt --header firmware/src/ui/assets.h
```

Then point `icon` at `icon_hello_bits`, and optionally give `anim` an array of
frames (ending in `nullptr`) the menu plays once when the app is selected.

## 6. Optional: talk to the server

If your app needs data from the companion server, reuse the connection the rest
of the device already shares — [`firmware/src/net/conn.h`](../firmware/src/net/conn.h)
gives you `conn::server_url()` and `conn::device_key()`. Add your route on the
[FastAPI side](../server/app/main.py), guard it with the same `X-Device-Key`
dependency as the others, and return **lean JSON** (the ESP32 parses with a small
ArduinoJson buffer — keep fields short).

## 7. Test it on the PC

Pure logic (parsing, formatting, state) can be tested with **no board**:

```bash
cd firmware
pio test -e native
```

Keep the testable logic in `lib/` (portable, no Arduino headers) and the drawing
in `apps/` — that split is what lets the PC run the same code the ESP32 does.

---

<p align="center">
  Built something? → <a href="../README.md"><b>📟 Back to the project</b></a>
</p>
