<h3 align="center">
  <a href="../README.md">📟 Meet the project</a>&nbsp;&nbsp;·&nbsp;&nbsp;🔧 Build your own
</h3>

<p align="center">
  🇬🇧 English&nbsp;&nbsp;·&nbsp;&nbsp;<a href="INSTALL.pt-br.md">🇧🇷 Português</a>
</p>

# Build &amp; install

From zero to a phone that's powered on, following the World Cup and talking to
Claude: **build the hardware → run the server → flash the firmware → join WiFi →
pair Claw'd**. In that order.

---
## 🧰 Bill of materials

| Qty | Item | Notes |
|---|---|---|
| 1 | ESP32 WROOM-32 DevKit (30 pins) | Any DevKit with a USB-serial bridge (CP2102/CH340) works |
| 1 | Nokia 5110 display (PCD8544) | Module with backlight, 8 pins |
| 1 | DS3231 RTC (ZS-042 module or similar) | Optional, but without it there's no alarm and no offline time |
| 1 | CR2032 battery | Lets the DS3231 keep time while powered off |
| 4 | Tactile push buttons | UP, DOWN, OK and C |
| 1 | **Passive** buzzer | Passive only — an active one won't play melodies |
| 1 | INMP441 I2S microphone | For Claw'd (push-to-talk to Claude) — optional if you skip voice |
| 1 | Breadboard (830 pts) + jumpers | Solderless build |

---
## 🔌 Pinout

The source of truth is [`firmware/include/pins.h`](../firmware/include/pins.h):

| Module | Signal | GPIO |
|---|---|---|
| Nokia 5110 | CLK | 18 |
| | DIN | 23 |
| | DC | 17 |
| | CE | 5 |
| | RST | 16 |
| | BL (backlight) | 4 |
| Buttons | UP | 13 |
| | DOWN | 14 |
| | OK | 27 |
| | C | 26 |
| Buzzer | + | 25 |
| DS3231 | SDA | 21 |
| | SCL | 22 |
| INMP441 mic | SCK | 33 |
| | WS | 32 |
| | SD | 35 |

- Display and DS3231 on **3V3** (the 5110 does not tolerate 5 V).
- Buttons between the GPIO and **GND** — the firmware uses internal pull-ups.
- Passive buzzer: positive on GPIO 25, negative on GND.
- INMP441 on **3V3**; tie its `L/R` pin to GND (left channel).

<p align="center">
  <img src="assets/photos/protoboard.png" width="360" alt="the breadboard build with all components wired up">
</p>

<sub>The full solderless build — every module on the breadboard.</sub>

---
## 🐍 Companion server

The server feeds the World Cup, the leagues and Claw'd. Run it on your machine,
a box at home, or any host that runs Python or a container — the firmware only
needs to reach the URL. **Bring your own keys: nothing is hosted for you.**

### Run it locally

```bash
cd server
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

cp .env.example .env        # edit it (see the table below)
export $(grep -v '^#' .env | xargs)
uvicorn app.main:app --host 0.0.0.0 --port 8000
```

### Run it with Docker

```bash
cd server
docker build -t espnokia-server .
docker run -p 8000:8000 \
  -e DEVICE_KEYS="*" \
  -e ANTHROPIC_API_KEY="sk-ant-..." \
  espnokia-server
```

### Environment variables

| Variable | What it does |
|---|---|
| `DEVICE_KEYS` | Keys accepted in the `X-Device-Key` header. Use `*` for **capability mode** (any strong key is accepted, data isolated per key) — the easy path when the device rolls its own key. A CSV of fixed keys also works. **Empty = auth off** (local dev only) |
| `ANTHROPIC_API_KEY` | Your Claude key — required for Claw'd. The persistent way to set it (a key typed in the dashboard only lasts while the server runs) |
| `GROQ_API_KEY` | Optional. Cloud speech-to-text; without it, STT runs locally with faster-whisper |
| `LIVE_SOURCE_URL` | Optional live-score source. Empty = off; the app still works from the table |
| `STT_PRELOAD` | Optional. `1` warms up the local whisper model at boot instead of on the first words |

### Smoke-check it

```bash
curl http://localhost:8000/health
# {"status":"ok"}

curl -H "X-Device-Key: any-strong-key-16+chars" "http://localhost:8000/copa/proximos?n=3"
# {"jogos":[...], "atualizado_s": 0}
```

With no key (or a weak/unknown one) the protected routes answer **401** — that's
the "API with a key" half of the project working.

### Server tests

```bash
pip install -r requirements-dev.txt
python -m pytest tests/        # 157 tests
```

---
## 📟 Firmware

Prerequisite: [PlatformIO](https://platformio.org/install/cli) (CLI or the
VS Code extension).

### 1. Configure the device

```bash
cd firmware
cp include/espnokia_config.example.h include/espnokia_config.h
```

`espnokia_config.h` is **gitignored on purpose** — secrets never reach the repo.
In v0.5.0 most of it is optional: the device **rolls its own device key** at
first boot and the **server URL is set from the phone** (no recompile). The
file only seeds the defaults:

```c
#define SERVER_URL  "http://192.168.0.10:8000"   // default URL; editable later on the device
#define DEVICE_KEY  ""                            // leave empty to let the device generate one
```

WiFi is **not** here — it's configured from your phone
([provisioning](#-joining-wifi)).

### 2. Test without a board

```bash
pio test -e native      # 82 tests running straight on the PC
```

### 3. Flash

```bash
pio run -e esp32dev -t upload
pio device monitor      # 115200 baud, to watch the boot
```

---
## 📶 Joining WiFi

On first power-up (or after **Settings → Connections → WiFi/Server → switch
network**) the device enters **config mode**:

1. The screen shows the network name (`espnokia-XXXX`) and an **8-digit numeric
   password** — drawn fresh each time you enter config mode, it only exists there.
2. Connect your phone to that network. The captive portal opens the page by
   itself (if it doesn't, go to `http://192.168.4.1`).
3. Tap your network in the list (padlock = protected; the bars show signal
   strength). **Hidden network?** Type its name in the manual field. Then enter
   the password and **SAVE** — the rescan button refreshes the list on the spot.
   The **server URL** field is on this same page.
4. The device reboots and connects. From here on, you just use it.

<p align="center">
  <img src="assets/prov-scan.png" width="340" alt="config page with the network scan">
  &nbsp;&nbsp;
  <img src="assets/prov-saved.png" width="340" alt="network-saved confirmation">
</p>

Your WiFi password is **encrypted in NVS** with a key derived from the chip's
own eFuse MAC. To wipe it: **Settings → Connections → WiFi/Server → forget**.

---
## 🐾 Pairing Claw'd &amp; the dashboard

Once the device is online and the server has an `ANTHROPIC_API_KEY`:

1. On the phone, go to **Settings → Connections → Device QR**. The screen shows
   a QR code built from your server URL and the device's own key.
2. Scan it with your phone's camera — the **dashboard opens already logged in**
   (the key rides in the URL fragment). Install it as a PWA if you like
   ("EspNokia Dash").
3. From the dashboard, set the **persona**, the **STT engine**, the **reply
   length**, read the pet's **memory** / recent **conversations**, or drop in
   your keys.
4. Open **Claw'd** on the phone, **hold the button and speak**, let go — the pet
   listens, thinks (the ✶ pulses) and reads Claude's reply back, page by page.

---
## 🚑 Common problems

| Symptom | Likely cause / fix |
|---|---|
| `pio run -t upload` can't find the port | Linux: join the `dialout` group (`sudo usermod -aG dialout $USER`, re-login). Check your board's USB-serial driver (CP2102/CH340) |
| Display lights up but shows nothing | Contrast/wiring: check CLK/DIN/DC/CE/RST against the [pinout](#-pinout); the 5110 is sensitive to a loose jumper |
| An app says "Connect WiFi" | The device has no network yet — run [provisioning](#-joining-wifi) |
| An app shows "NO RESPONSE" | The firmware didn't get a 200 from the server: check the server URL (right IP? right port? no trailing slash?), that the server is up (`curl .../health`), and that the device key is accepted (weak/unknown key = 401) |
| Claw'd never answers | The server has no `ANTHROPIC_API_KEY`, or the mic isn't wired (SCK 33 / WS 32 / SD 35, `L/R` to GND) |
| Clock has no time / no alarm | DS3231 missing or SDA/SCL swapped; without the RTC the clock runs but the alarm won't |
| Tones silent or one long tone | Active buzzer instead of passive, or reversed polarity |

---

<p align="center">
  Built it? → <a href="../README.md"><b>📟 Back to the project tour</b></a>
</p>
