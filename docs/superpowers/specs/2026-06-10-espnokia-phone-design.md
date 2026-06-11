# espnokia-phone — Design

**Data:** 2026-06-10
**Status:** aprovado para planejamento

## Visão

Um "Nokia OS" fake e carinhoso rodando num ESP32 WROOM com display Nokia 5110
(84×48 mono), trazendo modernidade pra estética de 2000: conversa por voz com
LLM (Claude), acompanhamento da Copa do Mundo 2026, ícones de status modernos
e OTA. O impacto é o contraste — "Claude num Nokia", "placar da Copa num
celular de botão".

**Este repo é o sistema:** a infra NokiaOS-style (shell, launcher, drivers,
API de apps) + dois apps de exemplo (Copa e Claude) + documentação. Projeto
público e replicável: o hardware é o que todo mundo já tem na gaveta (WROOM
comum, tela 5110 barata, botões, buzzer) — a única compra indispensável é o
mic I2S. O resto é firmware e carinho.

**Critério de pronto:** case 3D + README caprichado (capa, GIF, prints,
componentes, guia breve de configuração) + vídeo + post.

## Identidade

- Estética Nokia anos 90: menu monocromático em lista vertical, fontes pixel,
  navegação up/down/OK/C — fiel ao 3310 (sem teclado numérico).
- Toques de modernidade na status bar: ícone de WiFi, bateria, hora.
  (Bluetooth descartado — sem uso real no sistema.)
- Som 100% buzzer piezo — historicamente fiel (monofônico). Ringtones em
  **RTTTL** (formato do Nokia Composer — toca os toques originais).
- A "voz" do Claude é **beep-speech** (estilo Undertale/Animal Crossing):
  o texto da resposta aparece letra a letra com bipes de tom variável.

## Hardware (BOM)

| Item | Papel | Observação |
|---|---|---|
| ESP32 WROOM DevKit | host — WiFi + I2S + GPIOs | o ESP32 mais popular |
| Nokia 5110 LCD (PCD8544, SPI 3.3V) | display 84×48 | barata e icônica |
| INMP441 (mic I2S MEMS) | entrada de voz (push-to-talk) | única compra indispensável |
| 4× botão táctil | UP, DOWN, OK (central), C | estilo 3310 |
| Buzzer passivo | RTTTL + beep-speech + alarmes | |
| LiPo ~1100mAh + módulo carga/boost | portabilidade | qualquer um serve |

**Fora do BOM (decidido):** speaker/amp (beep-speech é a identidade), RTC
(NTP resolve), microSD, joystick, teclado numérico.

**Pendência de bancada:** confirmar se o módulo de carga entrega trilho
estável sob pico de WiFi TX; senão, módulo carga+proteção+boost dedicado.

## Layout físico (case 3D)

```
        ┌──────────────────────┐
        │  ┌────────────────┐  │
        │  │   NOKIA 5110   │  │
        │  └────────────────┘  │
        │                   ↑  │
        │   (C)   (OK)    ↓    │   ← C = voltar/cancelar
        │                      │
        │                      │
        │   (mic)    (buzzer)  │
        └──────────────────────┘
```

Push-to-talk: **segurar OK** dentro do app Claude. Navegação em lista
vertical (up/down), OK entra, C volta — exatamente o idioma do 3310.

## Pinagem (v4 — definitiva para protoboard)

Regras aplicadas: ADC só no ADC1 (ADC2 morre com WiFi); strapping pins
(0, 2, 12, 15) evitados em sinais críticos; botões em pinos com pull-up
interno (34–39 não têm); GPIO 6–11 intocáveis (flash); 1/3 livres (UART0).

| Função | GPIO | Nota |
|---|---|---|
| Display CLK | 18 | VSPI |
| Display DIN | 23 | VSPI MOSI |
| Display DC | 17 | |
| Display CE | 5 | CS, HIGH em repouso |
| Display RST | 16 | |
| Display BL | 4 | backlight via PWM (LEDC) |
| Mic SCK | 33 | I2S |
| Mic WS | 32 | I2S |
| Mic SD | 35 | input-only, perfeito p/ data-in |
| Botão UP | 19 | INPUT_PULLUP, ativo-baixo |
| Botão DOWN | 21 | idem |
| Botão OK | 22 | idem (push-to-talk no app Claude) |
| Botão C | 26 | idem (voltar/cancelar) |
| Buzzer | 25 | LEDC (tom variável) |
| VBAT sense | 36 | input-only ADC1, divisor de tensão |

Livres para futuro: 0 (boot), 2, 12, 13, 14, 15, 27, 34, 39.

Notas elétricas: 5110 é 3.3V nativo (sem level shifter); resistor ~330Ω no
backlight; INMP441 com L/R em GND (canal esquerdo).

## Arquitetura do sistema

```
 espnokia-phone ─────► ┌────────────────────────────────┐
   POST /talk?mode=text│  SERVER companion (FastAPI)    │ ◄── Claude API
   GET  /copa/*        │  self-hosted: Docker, roda em  │ ◄── STT (Whisper)
                       │  qualquer host de container    │ ◄── TTS (opcional)
                       │  ou VPS — escolha do usuário   │ ◄── dados da Copa
                       └────────────────────────────────┘     (fonte sem key)
```

### Server companion (`server/` neste repo — voltado pro NokiaOS)

Desenhado para o NokiaOS; genérico o bastante pra qualquer cliente HTTP, mas
sem compromisso com outros devices. Self-hosted: Dockerfile + `.env.example`,
o usuário hospeda onde quiser e configura as próprias chaves. **Nenhuma
plataforma de hospedagem específica é citada no repo.**

- **Stack:** FastAPI (Python), container único.
- **Endpoints:**
  - `POST /talk` — recebe áudio WAV/PCM 16kHz mono (HTTP chunked streaming);
    pipeline STT → Claude → resposta. `mode=text` (o que o NokiaOS usa)
    devolve JSON `{transcricao, resposta}`; `mode=voice` (capability
    opcional do server) devolve também áudio TTS.
  - `GET /copa/proximos` — próximos jogos da Copa 2026.
  - `GET /copa/brasil` — só jogos do Brasil.
  - `GET /copa/live` — jogo(s) rolando agora, com placar.
  - `GET /health`.
- **Auth:** header `X-Device-Key` (chave definida em env var) — endpoint
  aberto na internet gastaria créditos de API de quem hospeda.
- **LLM:** Claude via env var (default haiku — latência/custo); system prompt
  persona "telefone retrô", respostas curtas (cabem em 84×48).
- **STT:** Groq Whisper API (plano A; free tier, container leve);
  faster-whisper local é plano B.
- **TTS:** opcional, atrás de env var (`TTS_PROVIDER`); só usado por
  `mode=voice`.
- **Copa:** fonte **sem API key** — candidatos validados: openfootball
  worldcup.json (domínio público) e worldcup2026 REST API (free, live
  scores); provedor final escolhido na implementação, com cache de 30–60s
  e fallback "dados antigos". Quem replica o projeto não registra nada.
- **`.env.example`:** `ANTHROPIC_API_KEY`, `GROQ_API_KEY`, `CLAUDE_MODEL`,
  `DEVICE_KEYS`, `TTS_PROVIDER` (opcional).

### Firmware (`firmware/` neste repo)

- **Stack:** PlatformIO + Arduino core ESP32 + U8g2 (driver PCD8544,
  full buffer = 504 bytes).
- **Arquitetura mini-OS** — o sistema é a infra; apps são plugáveis:

```
firmware/src/
├── main.cpp        # setup + loop: poll input → tick do app ativo → render
├── shell/          # launcher (lista vertical), status bar, transições
├── apps/           # interface comum: on_enter / on_tick / on_input / on_exit
│   ├── app_clock   # home: hora (NTP), wifi, bateria
│   ├── app_claude  # push-to-talk → /talk → texto + beep-speech
│   └── app_copa    # /copa/* → listas, seleção de jogo, alarme RTTTL
├── drivers/        # buttons (debounce), buzzer (RTTTL + beep), mic (I2S)
└── net/            # wifi manager + http client (chunked upload, JSON)
```

- **API de apps:** cada app declara nome + ícone (XBM) + callbacks de
  lifecycle; o launcher monta a lista sozinho. Adicionar app = 1 módulo novo.
- **Áudio do mic:** streaming HTTP chunked direto do I2S pro server enquanto
  o usuário segura OK — duração ilimitada, RAM mínima.
- **Beep-speech:** render letra-a-letra (~30ms/char) com bipe LEDC por
  sílaba/caractere, pitch pseudo-aleatório em escala fixa.
- **Alarme de jogo:** app_copa agenda alarme local (epoch via NTP); ao
  disparar, fullscreen "JOGO AGORA: BRA × ..." + ringtone RTTTL.
- **Config WiFi/URL/device-key:** `config.h` (gitignored, com
  `config.example.h` versionado) no MVP; NVS depois.

## Error handling (estética retrô nos erros também)

- WiFi caiu → ícone de antena vazio + tela "SEM REDE" + retry com backoff.
- Server timeout/5xx → "REDE OCUPADA. TENTE DE NOVO." + beep triste.
- STT vazio (silêncio) → "NÃO OUVI. SEGURE OK E FALE."
- Fonte da Copa indisponível → último cache com aviso "DADOS ANTIGOS".

## Testes

- **Server:** pytest (mocks de Claude/STT/fonte da Copa), CI simples.
- **Firmware:** lógica pura (parsing JSON, formatação de tela, scheduler de
  alarme, RTTTL parser) testável em env `native` do PlatformIO; periféricos
  validados por smoke test em cada fase de montagem.

## Fases de execução

1. **F1 — Core na protoboard:** display + 4 botões + buzzer; shell navegável
   com app_clock; RTTTL tocando ringtone Nokia. *(sem rede ainda)*
2. **F2 — Server + Copa:** `server/` com Docker + `.env.example`, deploy
   (host à escolha de quem usa), app_copa com alarme. *(GET antes de áudio)*
3. **F3 — Claude por voz:** INMP441 + streaming chunked + `/talk?mode=text`
   + beep-speech. *(a demo principal)*
4. **F4 — Portátil + publicação:** bateria + VBAT, perfboard, case 3D, README
   com capa/GIF/prints/guia, vídeo, post.
5. **F5 — OTA:** atualização de firmware over-the-air — o charme moderno
   final num "Nokia".

## Decisões registradas (e porquês)

| Decisão | Porquê |
|---|---|
| ESP32 WROOM host | 2+ ADC1, WiFi+I2S, GPIOs de sobra; é o ESP32 que todo mundo tem |
| Arduino + U8g2 (não ESP-IDF) | melhor lib PCD8544, iteração rápida, acessível pra quem replica |
| 4 botões (up/down/OK/C) | idioma autêntico do 3310; menos furos na case; navegação em lista |
| Só buzzer (sem speaker/amp) | fiel aos Nokia monofônicos; beep-speech dá a "voz"; menos peça |
| Server voltado pro NokiaOS | repo coeso e publicável; sem compromisso com outros devices |
| Server host-agnóstico (Docker) | código puro pra qualquer um hospedar; repo não cita plataforma |
| Retorno em texto no device | digitar é inviável e não há speaker; texto+beep é a identidade |
| Streaming chunked do mic | WAV em RAM limita fala a ~5s; chunked = ilimitado |
| STT via Groq Whisper | container leve, latência baixa; faster-whisper é plano B |
| Copa por fonte sem API key | quem clona roda sem registrar nada |
| Sem RTC/SD/BT no MVP | NTP resolve hora; nada pra persistir; BT sem uso real |
