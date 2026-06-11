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
