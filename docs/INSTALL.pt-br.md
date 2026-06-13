<h3 align="center">
  <a href="../README.pt-br.md">📟 Conhecer o projeto</a>&nbsp;&nbsp;·&nbsp;&nbsp;🔧 Montar o seu
</h3>

<p align="center">
  <a href="INSTALL.md">🇬🇧 English</a>&nbsp;&nbsp;·&nbsp;&nbsp;🇧🇷 Português
</p>

# Montagem e instalação

Do zero até o aparelho ligado, acompanhando a Copa e conversando com o Claude:
**montar o hardware → subir o server → gravar o firmware → conectar no WiFi →
parear o Claw'd**. Nessa ordem.

## 🧰 Lista de materiais

| Qtde | Item | Observação |
|---|---|---|
| 1 | ESP32 WROOM-32 DevKit (30 pinos) | Qualquer DevKit com USB-serial (CP2102/CH340) serve |
| 1 | Display Nokia 5110 (PCD8544) | Módulo com backlight, 8 pinos |
| 1 | RTC DS3231 (módulo ZS-042 ou similar) | Opcional, mas sem ele não há alarme nem hora offline |
| 1 | Bateria CR2032 | Para o DS3231 segurar a hora desligado |
| 4 | Botão tátil (push button) | UP, DOWN, OK e C |
| 1 | Buzzer **passivo** | Passivo mesmo — o ativo não toca melodia |
| 1 | Microfone I2S INMP441 | Para o Claw'd (push-to-talk com o Claude) — opcional se dispensar a voz |
| 1 | Protoboard 830 pontos + jumpers | Montagem sem solda |

## 🔌 Pinagem

A fonte da verdade é [`firmware/include/pins.h`](../firmware/include/pins.h):

| Módulo | Sinal | GPIO |
|---|---|---|
| Nokia 5110 | CLK | 18 |
| | DIN | 23 |
| | DC | 17 |
| | CE | 5 |
| | RST | 16 |
| | BL (backlight) | 4 |
| Botões | UP | 13 |
| | DOWN | 14 |
| | OK | 27 |
| | C | 26 |
| Buzzer | + | 25 |
| DS3231 | SDA | 21 |
| | SCL | 22 |
| Mic INMP441 | SCK | 33 |
| | WS | 32 |
| | SD | 35 |

- Display e DS3231 em **3V3** (o 5110 não tolera 5 V).
- Botões entre o GPIO e o **GND** — o firmware usa pull-up interno.
- Buzzer passivo: positivo no GPIO 25, negativo no GND.
- INMP441 em **3V3**; ligue o pino `L/R` no GND (canal esquerdo).

## 🐍 Server companion

O server alimenta a Copa, as ligas e o Claw'd. Rode na sua máquina, num servidor
da sua casa ou em qualquer host que rode Python ou um container — o firmware só
precisa alcançar a URL. **Traga suas próprias chaves: nada é hospedado pra você.**

### Rodando local

```bash
cd server
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

cp .env.example .env        # edite (veja a tabela abaixo)
export $(grep -v '^#' .env | xargs)
uvicorn app.main:app --host 0.0.0.0 --port 8000
```

### Rodando com Docker

```bash
cd server
docker build -t espnokia-server .
docker run -p 8000:8000 \
  -e DEVICE_KEYS="*" \
  -e ANTHROPIC_API_KEY="sk-ant-..." \
  espnokia-server
```

### Variáveis de ambiente

| Variável | O que faz |
|---|---|
| `DEVICE_KEYS` | Chaves aceitas no header `X-Device-Key`. Use `*` pro **modo capability** (qualquer chave forte é aceita, dados isolados por chave) — o caminho fácil quando o aparelho sorteia a própria chave. Um CSV de chaves fixas também vale. **Vazio = auth desligada** (só em dev local) |
| `ANTHROPIC_API_KEY` | Sua chave do Claude — necessária pro Claw'd. É a forma persistente de definir (chave digitada no dashboard só vale enquanto o server roda) |
| `GROQ_API_KEY` | Opcional. Speech-to-text na nuvem; sem ela, o STT roda local com faster-whisper |
| `LIVE_SOURCE_URL` | Fonte opcional de placar ao vivo. Vazio = desligada; o app continua funcionando com a tabela |
| `STT_PRELOAD` | Opcional. `1` aquece o whisper local no boot em vez de na primeira fala |

### Conferindo

```bash
curl http://localhost:8000/health
# {"status":"ok"}

curl -H "X-Device-Key: uma-chave-forte-16+caracteres" "http://localhost:8000/copa/proximos?n=3"
# {"jogos":[...], "atualizado_s": 0}
```

Sem a chave (ou com uma fraca/desconhecida), as rotas protegidas respondem
**401** — é a parte "API com chave" do projeto funcionando.

### Testes do server

```bash
pip install -r requirements-dev.txt
python -m pytest tests/        # 157 testes
```

## 📟 Firmware

Pré-requisito: [PlatformIO](https://platformio.org/install/cli) (CLI ou
extensão do VS Code).

### 1. Configure o aparelho

```bash
cd firmware
cp include/espnokia_config.example.h include/espnokia_config.h
```

O `espnokia_config.h` é **gitignored de propósito** — segredo nunca vai pro
repositório. Na v0.5.0 quase tudo é opcional: o aparelho **sorteia a própria
chave de device** no primeiro boot e a **URL do server é definida pelo celular**
(sem recompilar). O arquivo só semeia os padrões:

```c
#define SERVER_URL  "http://192.168.0.10:8000"   // URL padrão; editável depois no aparelho
#define DEVICE_KEY  ""                            // vazio = o aparelho gera a própria chave
```

O WiFi **não** fica aqui — ele é configurado pelo celular
([provisionamento](#-conectando-no-wifi)).

### 2. Teste sem placa

```bash
pio test -e native      # 82 testes rodando direto no PC
```

### 3. Grave

```bash
pio run -e esp32dev -t upload
pio device monitor      # 115200 baud, pra acompanhar o boot
```

## 📶 Conectando no WiFi

Na primeira ligada (ou depois de **Config → Conexões → WiFi/Servidor → trocar de
rede**), o aparelho entra em **modo config**:

1. A telinha mostra o nome da rede (`espnokia-XXXX`) e uma **senha numérica de 8
   dígitos** — sorteada a cada entrada no modo config, só existe ali.
2. Conecte o celular nessa rede. O captive portal abre a página sozinho (se não
   abrir, vá em `http://192.168.4.1`).
3. Toque na sua rede na lista (cadeado = protegida; as barras mostram a
   intensidade do sinal). **Rede oculta?** Digite o nome dela no campo manual.
   Aí é só pôr a senha e **SALVAR** — o botão de buscar de novo refaz o scan na
   hora. O campo da **URL do server** fica nessa mesma página.
4. O aparelho reinicia e conecta. A partir daqui é só usar.

<p align="center">
  <img src="assets/prov-scan.png" width="340" alt="página de configuração com o scan de redes">
  &nbsp;&nbsp;
  <img src="assets/prov-saved.png" width="340" alt="confirmação de rede salva">
</p>

A senha da sua rede fica **cifrada na NVS** com chave derivada do MAC em eFuse
do próprio chip. Pra apagar: **Config → Conexões → WiFi/Servidor → esquecer**.

## 🐾 Pareando o Claw'd &amp; o dashboard

Com o aparelho online e o server com uma `ANTHROPIC_API_KEY`:

1. No aparelho, vá em **Config → Conexões → QR do aparelho**. A tela mostra um QR
   montado a partir da URL do server e da chave do próprio device.
2. Escaneie com a câmera do celular — o **dashboard abre já logado** (a chave
   viaja no fragmento da URL). Instale como PWA se quiser ("EspNokia Dash").
3. No dashboard, defina a **persona**, o **motor de STT**, o **limite de
   resposta**, leia a **memória** / **conversas** recentes do pet, ou coloque
   suas chaves.
4. Abra o **Claw'd** no aparelho, **segure o botão e fale**, solte — o pet
   escuta, pensa (a ✶ pulsa) e lê a resposta do Claude página por página.

## 🚑 Problemas comuns

| Sintoma | Causa provável / solução |
|---|---|
| `pio run -t upload` não acha a porta | Linux: entre no grupo `dialout` (`sudo usermod -aG dialout $USER`, relogue). Confira o driver USB-serial da placa (CP2102/CH340) |
| Display liga mas não mostra nada | Contraste/conexões: confira CLK/DIN/DC/CE/RST contra a [pinagem](#-pinagem); o 5110 é sensível a jumper frouxo |
| Um app pede "Conecte o WiFi" | O aparelho não tem rede configurada — faça o [provisionamento](#-conectando-no-wifi) |
| Um app mostra "SEM RESPOSTA" | O firmware não obteve um 200 do server: confira a URL (IP certo? porta certa? sem barra no fim?), se o server está de pé (`curl .../health`) e se a chave do device é aceita (chave fraca/desconhecida = 401) |
| O Claw'd nunca responde | O server está sem `ANTHROPIC_API_KEY`, ou o mic não está ligado (SCK 33 / WS 32 / SD 35, `L/R` no GND) |
| Relógio sem hora / sem alarme | DS3231 ausente ou SDA/SCL trocados; sem RTC o relógio funciona, mas alarme não |
| Toques sem som ou som contínuo | Buzzer ativo no lugar do passivo, ou polaridade invertida |

---

<p align="center">
  Montou? → <a href="../README.pt-br.md"><b>📟 Volta pro tour do projeto</b></a>
</p>
