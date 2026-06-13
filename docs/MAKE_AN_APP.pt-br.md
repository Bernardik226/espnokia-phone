<h3 align="center">
  <a href="../README.pt-br.md">📟 Voltar pro projeto</a>&nbsp;&nbsp;·&nbsp;&nbsp;🧩 Crie seu próprio app
</h3>

<p align="center">
  <a href="MAKE_AN_APP.md">🇬🇧 English</a>&nbsp;&nbsp;·&nbsp;&nbsp;🇧🇷 Português
</p>

# Crie seu próprio app

> Isto é um **direcionamento, não uma regra** — um caminho sugerido pra você
> adicionar a sua própria tela ao NokiaOS. O shell é pequeno de propósito; depois
> de ver um app, dá pra moldar o resto do seu jeito.

O NokiaOS é um **app shell**. Cada app é uma struct simples de callbacks que o
shell chama nos momentos certos — sem herança, sem framework. Adicionar um são
três passos pequenos: **escrever o app, dar um nome, registrar.**

## 1. O contrato

Todo app preenche o [`firmware/lib/nokiacore/app.h`](../firmware/lib/nokiacore/app.h).
Qualquer callback que você não precisar pode ser `nullptr`:

```cpp
struct App {
  uint8_t name_id;                         // o nome no launcher (um StrId do i18n)
  void (*on_enter)();                      // abriu — prepare o estado
  void (*on_tick)(uint32_t now_ms);        // todo loop — animar, consultar
  bool (*on_input)(Button b, BtnEvent e);  // tecla — retorne true se consumiu
  void (*on_exit)();                       // fechou — limpe
  void (*on_render)(void* gfx);            // desenhe um frame (cast pra U8G2*)
  const unsigned char* icon;               // XBM 28×24 pro menu (ou nullptr)
  const unsigned char* const* anim;        // frames 28×24 terminando em nullptr (ou nullptr)
};
```

`on_render` recebe um `void*` pra manter o core agnóstico de hardware; o glue
passa um `U8G2*`. Faça o cast uma vez e desenhe.

## 2. Um app mínimo

Crie `firmware/src/apps/app_hello.h`:

```cpp
#pragma once
#include "app.h"
extern const App app_hello;
```

…e `firmware/src/apps/app_hello.cpp`:

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
  return false;   // deixa o shell tratar C (voltar), UP/DOWN, etc.
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

Helpers que valem conhecer (veja [`nokia_ui.h`](../firmware/src/ui/nokia_ui.h)):
`text_bold_center`, `text_bold`, `softkey`. Pra texto com acento use `drawUTF8`
(`drawStr` para no ASCII 127). A fonte do corpo é `u8g2_font_3310_small`.

## 3. Dê um nome (i18n)

O launcher mostra o `name_id`, um id de string da tabela de i18n em
[`firmware/lib/i18n`](../firmware/lib/i18n). Adicione `STR_APP_HELLO` (e qualquer
outra string que o app exiba) lá, com uma tradução por idioma — `tr(id)` devolve
a certa em runtime. Manter o texto do usuário no i18n é o que mantém o aparelho
em 9 idiomas de graça.

## 4. Registre

No [`firmware/src/main.cpp`](../firmware/src/main.cpp), inclua o header e coloque
o app na lista do launcher:

```cpp
#include "apps/app_hello.h"

static const App* kApps[] = {&app_clock,   &app_claude, &app_esportes,
                             &app_weather, &app_tones,  &app_hello,
                             &app_settings};
```

Pronto — compile, grave, e o seu app está no menu.

## 5. Opcional: um ícone &amp; animação de seleção

Os ícones do menu são **bitmaps 28×24 escritos como texto** em
[`firmware/assets/assets.txt`](../firmware/assets/assets.txt) (um pequeno DSL de
grade — `#` é um pixel aceso) e compilados pra XBM pela ferramenta em
[`tools/`](../tools):

```bash
python3 tools/grid2xbm.py firmware/assets/assets.txt --header firmware/src/ui/assets.h
```

Depois aponte o `icon` pra `icon_hello_bits` e, se quiser, dê ao `anim` um array
de frames (terminando em `nullptr`) que o menu toca uma vez quando o app é
selecionado.

## 6. Opcional: falar com o server

Se o seu app precisa de dados do server companion, reaproveite a conexão que o
resto do aparelho já compartilha — [`firmware/src/net/conn.h`](../firmware/src/net/conn.h)
te dá `conn::server_url()` e `conn::device_key()`. Adicione a sua rota no
[lado FastAPI](../server/app/main.py), proteja com a mesma dependência
`X-Device-Key` das outras, e responda **JSON enxuto** (o ESP32 parseia com um
buffer pequeno do ArduinoJson — mantenha os campos curtos).

## 7. Teste no PC

Lógica pura (parsing, formatação, estado) dá pra testar **sem placa**:

```bash
cd firmware
pio test -e native
```

Mantenha a lógica testável em `lib/` (portável, sem headers do Arduino) e o
desenho em `apps/` — essa divisão é o que deixa o PC rodar o mesmo código que o
ESP32 roda.

---

<p align="center">
  Fez algo? → <a href="../README.pt-br.md"><b>📟 Voltar pro projeto</b></a>
</p>
