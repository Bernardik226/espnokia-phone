<p align="center">
  <img src="docs/assets/banner.png" alt="ESPNOKIA" width="720">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-00599C?logo=cplusplus&logoColor=white" alt="C++">
  <img src="https://img.shields.io/badge/Python-3776AB?logo=python&logoColor=white" alt="Python">
  <img src="https://img.shields.io/badge/PlatformIO-FF7F00?logo=platformio&logoColor=white" alt="PlatformIO">
  <img src="https://img.shields.io/badge/ESP32-E7352C?logo=espressif&logoColor=white" alt="ESP32">
</p>

# espnokia-phone

📟 Um "Nokia OS" fake e carinhoso num ESP32 + display Nokia 5110 — estética de
2000, recursos de 2026 (em construção: app da Copa 2026 e conversa por voz
com Claude).

**Status:** F1.5 — shell navegável (relógio, toques RTTTL, configurações). 🚧

## Destaques

- Boot fiel ao Nokia 1100: pixel art das mãos + startup chime no buzzer 🎵
- 9 toques originais de fábrica da Nokia, transcritos em RTTTL (parser próprio)
- Backlight em PWM com 3 níveis, persistido na NVS
- 19 testes unitários que rodam sem hardware

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
