<p align="center">
  <img src="docs/assets/banner.png" alt="ESPNOKIA" width="680">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/build-passing-2ea44f?style=flat-square" alt="build">
  <img src="https://img.shields.io/badge/tests-99_native_·_157_server-1e5aa8?style=flat-square" alt="tests">
  <img src="https://img.shields.io/badge/deploy-Railway-8b5cf6?style=flat-square" alt="deploy Railway">
  <img src="https://img.shields.io/badge/license-fan_·_educational-f47b20?style=flat-square" alt="license">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/ESP32-WROOM--32-1e5aa8?style=for-the-badge&logo=espressif&logoColor=white" alt="ESP32">
  <img src="https://img.shields.io/badge/C%2B%2B17-firmware-f47b20?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++17">
  <img src="https://img.shields.io/badge/FastAPI-server-1e5aa8?style=for-the-badge&logo=fastapi&logoColor=white" alt="FastAPI">
  <img src="https://img.shields.io/badge/PlatformIO-build-f47b20?style=for-the-badge&logo=platformio&logoColor=white" alt="PlatformIO">
</p>

<p align="center">
  🇬🇧 English&nbsp;&nbsp;·&nbsp;&nbsp;<a href="README.pt-br.md">🇧🇷 Português</a>&nbsp;&nbsp;·&nbsp;&nbsp;<a href="docs/INSTALL.md">🔧 Build yours</a>&nbsp;&nbsp;·&nbsp;&nbsp;<a href="docs/ARQUITETURA.md">🏛️ Architecture</a>&nbsp;&nbsp;·&nbsp;&nbsp;<a href="docs/MAKE_AN_APP.md">🧩 Make an app</a>
</p>

---

<p align="center"><b>A Nokia 3310 that talks to Claude — firmware, printed case, backend and PWA, all built by me.</b></p>

I built this handheld from scratch to fold everything I wanted to show into one project: **embedded firmware** on an ESP32, a **case I designed and printed**, an **AI-powered FastAPI backend** and a **configuration PWA**. It's my end-to-end portfolio — from the pixel on the 84×48 screen to the container on deploy. Year-2000 looks, 2026 features.

<!-- PHOTO — uncomment after adding docs/assets/foto-espnokia-01.jpg:
<p align="center">
  <img src="docs/assets/foto-espnokia-01.jpg" width="420" alt="assembled espnokia — the printed, powered device on a desk">
</p>
-->

<p align="center">
  <img src="docs/assets/boot-hands.gif" width="240" alt="the recreated Nokia boot animation: two pixel hands meet and the logo lights up">
</p>

---

## ✨ What it does

- 🐾 **Claw'd — talk to Claude.** Hold the button, speak, release: the mic captures, the server transcribes, **Claude answers**, and a pixel-art critter reads the reply on the little screen. Every exchange becomes a **log**, and a rolling **memory** lets the pet remember you across reboots.
- 🏆 **World Cup 2026, live.** Upcoming matches, Brazil's games, a live panel and group tables. Flag a match and the device **rings when it kicks off**; the score updates on its own and **"GOAL!" flashes on screen** the instant it changes.
- ⚽ **Club leagues.** Champions League, Libertadores and friends, with a standings table that **adapts to the competition** (numbered league table / navigable groups).
- ⏰ **3310-style daily life.** A clock with an **alarm that survives reboots** and a timer, ambient temperature from the DS3231, and **9 original Nokia ringtones** in RTTTL.
- 🐍 **Snake II.** The classic, with its own engine tested on the PC.
- 📶 **WiFi with no recompile.** The device becomes an access point with a captive portal and a freshly-drawn password — switch network and server URL with no cable and no reflash.
- 🌍 **9 languages** across the whole system; **99 native tests** of pure logic + **157 on the server**.

<table align="center">
<tr>
<td align="center" width="50%">
  <img src="docs/assets/clawd-voice.gif" width="380" alt="push-to-talk with Claw'd: listening, thinking, answering"><br>
  <b>🐾 Claw'd</b> — hold, speak, Claude answers
</td>
<td align="center" width="50%">
  <img src="docs/assets/copa-nav.gif" width="380" alt="navigating the World Cup app on the device"><br>
  <b>🏆 WC 26</b> — live on an 84×48 screen
</td>
</tr>
</table>

<!-- PHOTO — uncomment after adding docs/assets/foto-espnokia-02.jpg:
<p align="center">
  <img src="docs/assets/foto-espnokia-02.jpg" width="420" alt="espnokia in use — the monochrome screen running one of the apps">
</p>
-->

**Configuration via the PWA.** An installable web panel (the "EspNokia Dash") **configures the device**: pair/discover by QR, see the **online status**, pick **Claude's personality**, the transcription engine, and drop in the **AI key** — with no config screen on the Nokia itself.

---

## 🔍 How it works

It's **three pieces** that talk over an authenticated API (`X-Device-Key`). The ESP32 is a plain chip, **no PSRAM** and with little RAM/flash — so it stores no history and transcribes nothing: it keeps only fixed buffers, a state machine, and **paginates** what it needs. All the durable state (the pet's memory, transcription, Claude calls) lives on the **server**, which **summarizes** old conversations so the context never grows without bound.

> **The core idea:** a *thin client* on the ESP32 (fixed, byte-capped buffers) + a *fat server* that holds and summarizes the history. That's how the firmware fits in **~⅓ of the flash and ¼ of the RAM** (flash 36.1% / RAM 25.6%).

```mermaid
flowchart LR
    D["📟 Firmware (ESP32)<br/>thin client"]
    S["🐍 Server (FastAPI · Docker)<br/>fat server"]
    P["📱 PWA<br/>configuration"]
    D -- "POST /claude/voz · PCM16<br/>GET /copa /futebol /claude/registro" --> S
    S -- "lean JSON · paginated text" --> D
    P -- "/admin/* · X-Device-Key" --> S
```

Architecture and technical decisions → **[docs/ARQUITETURA.md](docs/ARQUITETURA.md)** (in Portuguese for now)

---

## 🔌 Hardware

| | Component | Role |
|---|---|---|
| <img src="docs/assets/components/comp-esp32.png" width="60" alt="ESP32"> | **ESP32 WROOM-32** DevKit, 30 pins | Brain: WiFi, 2 cores, the whole NokiaOS |
| <img src="docs/assets/components/comp-5110.png" width="60" alt="Nokia 5110"> | **Nokia 5110 display** (PCD8544) | 84×48 monochrome — the real Nokia panel, over SPI |
| <img src="docs/assets/components/comp-ds3231.png" width="60" alt="DS3231"> | **DS3231 RTC** | Battery-backed time + on-board thermometer, over I2C |
| <img src="docs/assets/components/comp-button.png" width="60" alt="buttons"> | **4 tactile buttons** | UP · DOWN · OK · C — full 3310-style navigation |
| <img src="docs/assets/components/comp-buzzer.png" width="60" alt="buzzer"> | **Passive buzzer** | RTTTL ringtones, beeps and the goal alert (volume via PWM) |
| 🎤 | **I2S mic INMP441** | Captures your voice for Claw'd |
| <img src="docs/assets/components/comp-proto.png" width="60" alt="breadboard"> | **Breadboard + jumpers** | Solderless build — full pinout in [`docs/INSTALL.md`](docs/INSTALL.md) |

---

## 🚀 Run / build

```bash
# Firmware (PlatformIO) — from firmware/
cd firmware
pio run  -e esp32dev     # builds and flashes the ESP32
pio test -e native       # 99 pure-logic tests, run on the PC

# Server (FastAPI) — from server/
cd server
docker build -t espnokia .
docker run -p 8000:8000 --env-file .env espnokia   # ANTHROPIC_API_KEY is the only required one
```

**Deploy:** the plain `Dockerfile` runs on any container host — it's published on **Railway**. Then just open *Settings → Connections* on the device and point it at the server URL (no reflash). Full build guide in **[docs/INSTALL.md](docs/INSTALL.md)**.

---

## 🙏 Credits & attributions

This project stands on open work, with thanks:

- **Data** — [openfootball](https://github.com/openfootball/world-cup) (public-domain World Cup dataset) · **ESPN** (live scores and leagues via public endpoints; data © ESPN / The Walt Disney Company).
- **AI & voice** — [Anthropic Claude](https://www.anthropic.com/) (Claw'd's brain) · [faster-whisper](https://github.com/SYSTRAN/faster-whisper) (local STT) · [Groq](https://groq.com/) (cloud STT, optional).
- **Art & fonts** — [nokia-3310-fonts](https://git.janouch.name/p/nokia-3310-fonts) by **Premysl Janouch** · the Nokia 1100 boot and factory ringtones reproduced as an homage.
- **Libraries** — [U8g2](https://github.com/olikraus/u8g2) · [ArduinoJson](https://arduinojson.org/) · [QRCode](https://github.com/ricmoo/QRCode) · [FastAPI](https://fastapi.tiangolo.com/) · [Uvicorn](https://www.uvicorn.org/) · [httpx](https://www.python-httpx.org/) · [Anthropic SDK](https://github.com/anthropics/anthropic-sdk-python).

---

## ⚖️ Trademarks & authorship

**Nokia** and the Nokia phone designs are trademarks of **Nokia Corporation**. **Claude** and **Anthropic** are trademarks of **Anthropic, PBC**. The **FIFA World Cup** name and emblem are trademarks of **FIFA**. **Groq**, **ESPN** and other names belong to their respective owners. This is an independent, **non-commercial, fan & educational** project — a tribute — and is **not affiliated with, endorsed by, or sponsored by** any of them.

Original code, firmware, pixel art and assets © 2026 **Bernardo Melo**. Third-party components keep their own licenses.

<p align="center">
  Want one on your desk? → <a href="docs/INSTALL.md"><b>🔧 Build guide</b></a>
  &nbsp;·&nbsp; <img src="docs/assets/star-orange.png" width="16" alt="orange star"> built by <b>Bernardo Melo</b>
</p>
