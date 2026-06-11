# F2 — Server companion + app Copa — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Server FastAPI self-hosted com endpoints `/copa/*` e `/health` (Docker + `.env.example`) e, no firmware, WiFi + NTP→RTC + cliente HTTP + app Copa com alarme de jogo em RTTTL.

**Architecture:** O server agrega duas fontes sem API key — openfootball worldcup.json (tabela confiável, cache 30 min, domínio público) e worldcup26.ir (placar ao vivo, opcional, cache 60 s, fallback gracioso) — e entrega JSON compacto pro device (horários já em hora de Brasília). No firmware, um módulo `net/` (wifi manager + http + NTP) alimenta o `app_copa`; o alarme de jogo vive fora dos apps (módulo checado no loop principal contra o RTC) pra disparar mesmo no standby.

**Tech Stack:** FastAPI + httpx + pytest (server, container único Python 3.12-slim); Arduino core ESP32 + ArduinoJson 7 (firmware; parsing testável no env `native`).

**Contrato device↔server** (usado em vários tasks):

```json
GET /copa/proximos?n=8 | /copa/brasil | /copa/live   (header X-Device-Key)
{"jogos": [{"dia": 13, "mes": 6, "h": 19, "m": 0,
            "t1": "BRA", "t2": "MAR", "info": "Grupo C",
            "s1": -1, "s2": -1, "live": false}],
 "atualizado_s": 42}
```

- Horários em America/Sao_Paulo (o server converte de "18:00 UTC-4").
- `s1`/`s2 = -1` → sem placar. `atualizado_s` = idade do cache (segundos).
- Times nos códigos FIFA (48 seleções mapeadas); placeholders do mata-mata
  ("2A", "W101") passam como estão.

---

### Task 1: Esqueleto do server + /health

**Files:**
- Create: `server/app/__init__.py`, `server/app/main.py`
- Create: `server/requirements.txt`, `server/requirements-dev.txt`
- Create: `server/tests/test_health.py`
- Create: `server/.env.example`, `server/Dockerfile`, `server/.dockerignore`

- [ ] **Step 1: requirements**

`server/requirements.txt`:
```
fastapi==0.115.*
uvicorn[standard]==0.34.*
httpx==0.28.*
```
`server/requirements-dev.txt`:
```
-r requirements.txt
pytest==8.*
```

- [ ] **Step 2: teste do /health (falhando)**

`server/tests/test_health.py`:
```python
from fastapi.testclient import TestClient
from app.main import create_app

def test_health_aberto_sem_chave():
    client = TestClient(create_app())
    r = client.get("/health")
    assert r.status_code == 200
    assert r.json()["status"] == "ok"
```

Run: `cd server && python -m pytest -q` → FAIL (app.main não existe)

- [ ] **Step 3: app factory mínima**

`server/app/main.py`:
```python
from fastapi import FastAPI

def create_app() -> FastAPI:
    app = FastAPI(title="espnokia server", docs_url=None, redoc_url=None)

    @app.get("/health")
    def health():
        return {"status": "ok"}

    return app

app = create_app()
```

- [ ] **Step 4: testes passam** — `python -m pytest -q` → 1 passed

- [ ] **Step 5: Docker + env example**

`server/.env.example`:
```
# chave(s) que o(s) device(s) mandam no header X-Device-Key (csv)
# vazio = auth desligada (so pra dev local)
DEVICE_KEYS=troque-esta-chave

# fonte ao vivo opcional (vazio = desligada; so a tabela openfootball)
LIVE_SOURCE_URL=https://worldcup26.ir
```

`server/Dockerfile`:
```dockerfile
FROM python:3.12-slim
WORKDIR /srv
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY app ./app
EXPOSE 8000
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
```

`server/.dockerignore`: `tests/`, `__pycache__/`, `.env`

- [ ] **Step 6: Commit** — `feat(server): esqueleto FastAPI com /health, Docker e .env.example`

---

### Task 2: Parser openfootball → modelo interno

**Files:**
- Create: `server/app/sources/__init__.py`, `server/app/sources/openfootball.py`
- Create: `server/app/teams.py`
- Create: `server/tests/fixtures/worldcup_sample.json` (recorte real: 6 jogos de grupo + 2 de mata-mata do arquivo 2026)
- Test: `server/tests/test_openfootball.py`

- [ ] **Step 1: testes do parser (falhando)**

`server/tests/test_openfootball.py`:
```python
import json, pathlib
from app.sources.openfootball import parse_matches

FIX = pathlib.Path(__file__).parent / "fixtures" / "worldcup_sample.json"

def carrega():
    return parse_matches(json.loads(FIX.read_text()))

def test_converte_horario_para_brasilia():
    # fixture tem "2026-06-13" "18:00 UTC-4" (Brazil x Morocco) → 19:00 BRT
    m = [x for x in carrega() if x.t1 == "BRA"][0]
    assert (m.dia, m.mes, m.h, m.m) == (13, 6, 19, 0)

def test_mapeia_nomes_para_codigos_fifa():
    nomes = {(x.t1, x.t2) for x in carrega()}
    assert ("BRA", "MAR") in nomes

def test_placeholder_do_mata_mata_passa_direto():
    ko = [x for x in carrega() if x.info.startswith("Round")]
    assert ko and ko[0].t1 == "2A"

def test_sem_placar_vira_menos_um():
    m = carrega()[0]
    assert m.s1 == -1 and m.s2 == -1

def test_epoch_utc_correto():
    m = [x for x in carrega() if x.t1 == "BRA"][0]
    assert m.epoch == 1786395600  # 2026-06-13 22:00:00 UTC
```

Run: `python -m pytest tests/test_openfootball.py -q` → FAIL

- [ ] **Step 2: tabela de códigos**

`server/app/teams.py` — dict `NAME_TO_CODE` com as 48 seleções do arquivo
2026 (ex.: `"Brazil": "BRA", "Morocco": "MAR", "South Africa": "RSA",
"South Korea": "KOR", ...`) e:
```python
def code(name: str) -> str:
    return NAME_TO_CODE.get(name, name)  # placeholder do mata-mata passa direto
```
(Os 48 nomes saem de `jq` no arquivo real; conferir 1 a 1 com os códigos FIFA.)

- [ ] **Step 3: parser**

`server/app/sources/openfootball.py`:
```python
import re
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from zoneinfo import ZoneInfo

from app.teams import code

BRT = ZoneInfo("America/Sao_Paulo")
URL = "https://raw.githubusercontent.com/openfootball/worldcup.json/master/2026/worldcup.json"

@dataclass
class Match:
    epoch: int          # kickoff em UTC
    dia: int; mes: int; h: int; m: int   # kickoff em BRT
    t1: str; t2: str
    info: str           # "Grupo C" / "Round of 32" / "Final"
    s1: int = -1; s2: int = -1

def _kickoff(date_s: str, time_s: str) -> datetime:
    # "18:00 UTC-4" / "13:00 UTC-6"
    mt = re.fullmatch(r"(\d{2}):(\d{2}) UTC([+-]\d+)", time_s)
    hh, mm, off = int(mt[1]), int(mt[2]), int(mt[3])
    naive = datetime.strptime(date_s, "%Y-%m-%d").replace(hour=hh, minute=mm)
    return naive.replace(tzinfo=timezone(timedelta(hours=off)))

def _score(m: dict, key: str) -> int:
    v = m.get(key)
    return int(v) if v is not None else -1

def parse_matches(data: dict) -> list[Match]:
    out = []
    for m in data["matches"]:
        ko = _kickoff(m["date"], m["time"])
        loc = ko.astimezone(BRT)
        grupo = m.get("group")
        info = grupo.replace("Group", "Grupo") if grupo else m.get("round", "")
        out.append(Match(
            epoch=int(ko.timestamp()),
            dia=loc.day, mes=loc.month, h=loc.hour, m=loc.minute,
            t1=code(m["team1"]), t2=code(m["team2"]), info=info,
            s1=_score(m, "score1"), s2=_score(m, "score2")))
    out.sort(key=lambda x: x.epoch)
    return out
```

- [ ] **Step 4: fixture com recorte real** — copiar do arquivo baixado os 3
jogos do Brasil + abertura + 2 jogos do Grupo A + jogos 73/74 (Round of 32),
mantendo o formato original.

- [ ] **Step 5: testes passam** — `python -m pytest -q`

- [ ] **Step 6: Commit** — `feat(server): parser openfootball com fuso de Brasilia e codigos FIFA`

---

### Task 3: Busca com cache + fallback "dados antigos"

**Files:**
- Create: `server/app/fetcher.py`
- Test: `server/tests/test_fetcher.py`

- [ ] **Step 1: testes (falhando)** — `CachedFetcher(url, ttl_s)` com
`httpx.MockTransport`:

```python
def test_usa_cache_dentro_do_ttl(): ...      # 2ª chamada não bate na rede
def test_refaz_apos_ttl(): ...               # clock fake avança além do TTL
def test_erro_de_rede_serve_dado_antigo():   # upstream 500 → retorna cache + idade
def test_sem_cache_e_sem_rede_levanta(): ... # primeira chamada falhando → exceção
```

- [ ] **Step 2: implementação**

`server/app/fetcher.py`:
```python
import time
import httpx

class CachedFetcher:
    """GET JSON com cache em memória; em erro de upstream serve o último bom."""
    def __init__(self, url: str, ttl_s: int, *, transport=None, clock=time.monotonic):
        self.url, self.ttl_s, self.clock = url, ttl_s, clock
        self._client = httpx.Client(transport=transport, timeout=5.0)
        self._data = None
        self._at = 0.0

    def get(self):
        """retorna (json, idade_s)"""
        age = self.clock() - self._at
        if self._data is not None and age < self.ttl_s:
            return self._data, int(age)
        try:
            r = self._client.get(self.url)
            r.raise_for_status()
            self._data, self._at = r.json(), self.clock()
            return self._data, 0
        except Exception:
            if self._data is not None:
                return self._data, int(age)   # dados antigos > nada
            raise
```

- [ ] **Step 3: testes passam**
- [ ] **Step 4: Commit** — `feat(server): fetch com cache e fallback de dados antigos`

---

### Task 4: Serviço Copa (próximos / Brasil / ao vivo)

**Files:**
- Create: `server/app/copa.py`
- Test: `server/tests/test_copa.py`

Regras:
- `proximos(n)` → os `n` primeiros jogos com `epoch + 130*60 > agora`
  (jogo rolando ainda conta como "próximo" até acabar a janela).
- `brasil()` → só jogos com `"BRA"` em t1/t2 (mesma janela; inclui passados
  com placar? **Não** — mesma regra de próximos, simples).
- `live()` → jogos com `epoch <= agora <= epoch + 130*60`, marcados
  `live=True`; placar vem do enriquecimento (Task 5) quando disponível.
- Clock injetável (`now_fn`) pra teste.

- [ ] **Step 1: testes (falhando)** — fixtures de `Match` sintéticos +
`now_fn` fake; casos: ordenação, janela do live, filtro BRA, n máximo (20).
- [ ] **Step 2: implementação** (`CopaService` recebe fetcher + parser + now_fn)
- [ ] **Step 3: testes passam**
- [ ] **Step 4: Commit** — `feat(server): servico copa com proximos, brasil e janela de jogo ao vivo`

---

### Task 5: Enriquecimento ao vivo (opcional, à prova de queda)

**Files:**
- Create: `server/app/sources/livescore.py`
- Test: `server/tests/test_livescore.py`

- O cliente do worldcup26.ir (`GET {base}/get/games`, cache 60 s) casa jogos
  por (dia UTC, par de times via `fifa_code` upstream) e devolve
  `{(t1, t2): (s1, s2)}` só dos não-finalizados em andamento.
- **Qualquer erro → dict vazio** (a fonte caiu no dia da abertura; o server
  nunca pode depender dela). `LIVE_SOURCE_URL` vazio desliga o módulo.
- Teste com `MockTransport`: upstream ok, upstream 500 (→ `{}`), desligado (→ `{}`).

- [ ] **Step 1: testes (falhando)**
- [ ] **Step 2: implementação**
- [ ] **Step 3: testes passam**
- [ ] **Step 4: Commit** — `feat(server): placar ao vivo opcional com fallback silencioso`

---

### Task 6: Rotas /copa/* + auth X-Device-Key

**Files:**
- Modify: `server/app/main.py`
- Create: `server/app/auth.py`
- Test: `server/tests/test_api.py`

- `create_app(copa_service=None, device_keys=None)` aceita injeção pra teste;
  default lê env (`DEVICE_KEYS` csv, `LIVE_SOURCE_URL`).
- Auth: dependency que compara `X-Device-Key` com o csv; **`DEVICE_KEYS`
  vazio = auth desligada** (dev local); `/health` sempre aberto.
- Rotas serializam `Match` → contrato do device (`dia, mes, h, m, t1, t2,
  info, s1, s2, live`) + `atualizado_s`.
- Testes: 401 sem chave, 200 com chave, shape do JSON, `?n=` clampado.

- [ ] **Step 1: testes (falhando)**
- [ ] **Step 2: implementação**
- [ ] **Step 3: testes passam**
- [ ] **Step 4: smoke real (manual)** — `uvicorn app.main:app` + `curl
  -H "X-Device-Key: ..." localhost:8000/copa/proximos` contra a internet de
  verdade; conferir BRA x MAR 13/06 19:00.
- [ ] **Step 5: Commit** — `feat(server): rotas copa com auth por chave de device`

---

### Task 7: CI simples

**Files:**
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: workflow** — job 1: `pip install -r server/requirements-dev.txt
  && python -m pytest server/tests -q`; job 2: `pio test -e native` em
  `firmware/` (usa `pipx install platformio`). Trigger: push/PR no master.
- [ ] **Step 2: Commit** — `ci: pytest do server e testes nativos do firmware`

---

### Task 8: Firmware — config + WiFi manager + barras reais no standby

**Files:**
- Create: `firmware/include/config.example.h`
- Modify: `.gitignore` (adicionar `firmware/include/config.h`)
- Create: `firmware/src/net/wifi.h`, `firmware/src/net/wifi.cpp`
- Modify: `firmware/src/main.cpp`, `firmware/src/apps/app_standby.cpp`

- [ ] **Step 1: config.example.h**

```cpp
#pragma once
// copie para config.h (gitignored) e preencha
#define WIFI_SSID   "minha-rede"
#define WIFI_PASS   "minha-senha"
#define SERVER_URL  "http://192.168.0.10:8000"  // sem barra no fim
#define DEVICE_KEY  "troque-esta-chave"
```

- [ ] **Step 2: wifi manager** — `wifi::init()` (WiFi.begin não-bloqueante),
`wifi::connected()`, `wifi::level()` (RSSI→0..4: ≥-55→4, ≥-65→3, ≥-75→2,
≥-85→1, senão 0; desconectado→0), `wifi::tick()` (reconexão com backoff
10 s). Chamar `init` no setup e `tick` no loop.
- [ ] **Step 3: standby** — `wifi_level()` deixa de ser stub e vira
`wifi::level()`; ícone WiFi já existe.
- [ ] **Step 4: build + bancada** — barras sobem quando conecta.
- [ ] **Step 5: Commit** — `feat(net): wifi manager com reconexao e barras de sinal reais`

---

### Task 9: Firmware — NTP acerta o RTC (quando o toggle deixa)

**Files:**
- Create: `firmware/src/net/ntp.h`, `firmware/src/net/ntp.cpp`
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: módulo** — após conectar: se `settings_ntp_sync_enabled()`,
`configTime(-3*3600, 0, "pool.ntp.org")` (Brasília, sem horário de verão);
quando `getLocalTime` der certo → `rtc::write()` com a hora obtida e marca
re-sync pra +24 h. `ntp::tick()` no loop.
- [ ] **Step 2: bancada** — ligar o toggle nas Config, ver `[ntp]` no serial
e a hora do RTC corrigida.
- [ ] **Step 3: Commit** — `feat(net): sincronizacao NTP gravando no RTC, respeitando o toggle das config`

---

### Task 10: Firmware — HTTP client + parser do contrato (native-testável)

**Files:**
- Modify: `firmware/platformio.ini` (lib_deps += `bblanchon/ArduinoJson@^7`)
- Create: `firmware/lib/copamodel/copamodel.h`, `firmware/lib/copamodel/copamodel.cpp`
- Create: `firmware/test/test_copamodel/main.cpp`
- Create: `firmware/src/net/http.h`, `firmware/src/net/http.cpp`

- [ ] **Step 1: testes do parser (falhando)** — `copa_parse(json, jogos[],
max)` → n jogos; casos: payload normal, placar -1, truncamento em max,
JSON inválido → 0, labels (`copa_linha()` → `"13/6 BRA x MAR"`,
`"BRA 2x1 MAR"` quando tem placar).
- [ ] **Step 2: structs + parser com ArduinoJson** (funciona no env native).
- [ ] **Step 3: testes passam** — `pio test -e native`.
- [ ] **Step 4: http client** — `http::get_json(path, buf, len)`: HTTPClient,
header `X-Device-Key`, timeout 5 s, retorna código HTTP; URL base de config.h.
- [ ] **Step 5: Commit** — `feat(net): cliente http e modelo copa testavel em native`

---

### Task 11: Firmware — app Copa (listas + detalhe + ícone)

**Files:**
- Create: `firmware/src/apps/app_copa.h`, `firmware/src/apps/app_copa.cpp`
- Modify: `firmware/src/main.cpp` (registrar), `firmware/assets/assets.txt`
  + `firmware/src/ui/assets.h` (ícone troféu 28×24)

- [ ] **Step 1: ícone troféu** (gerador procedural, mesmo pipeline dos outros).
- [ ] **Step 2: app** — views: V_MENU (`Proximos`, `Brasil`, `Ao vivo` —
draw_list padrão), V_LIST (janela de 3 com os jogos; busca no on_enter da
view com tela "Buscando..."; erro → "REDE OCUPADA" + beep triste), V_DETAIL
(jogo: times, dia/hora, info, placar se houver; softkey `Avisar`).
- [ ] **Step 3: bancada** — navegar listas com o server rodando local.
- [ ] **Step 4: Commit** — `feat(copa): app copa com proximos, brasil e ao vivo`

---

### Task 12: Firmware — alarme de jogo (dispara até no standby)

**Files:**
- Create: `firmware/src/alarm.h`, `firmware/src/alarm.cpp`
- Modify: `firmware/src/main.cpp`, `firmware/src/apps/app_copa.cpp`

- [ ] **Step 1: módulo** — `alarm::set(dia, mes, h, m, label)` (persiste na
NVS), `alarm::clear()`, `alarm::active()`, `alarm::tick()` compara com
`rtc::now()` (janela de 5 min pra não perder por reboot); ao disparar:
fullscreen "JOGO AGORA" + label + ringtone RTTTL (Nokia tune) em loop até
OK/C. Overlay desenhado no main.cpp **depois** do render do app; input é
engolido enquanto ativo.
- [ ] **Step 2: integração no app_copa** — softkey `Avisar` no detalhe seta o
alarme (e mostra `Aviso ON`); detalhe de jogo com aviso setado permite
cancelar.
- [ ] **Step 3: bancada** — setar alarme 2 min no futuro pelas Config de data
(acelerar o relógio), ver fullscreen + toque no standby.
- [ ] **Step 4: Commit** — `feat(copa): alarme de jogo com ringtone, disparando do standby`

---

### Task 13: Fechamento da fase

- [ ] **Step 1: version bump** — `0.4.0  // F2: wifi, ntp->rtc, server copa, app copa com alarme`.
- [ ] **Step 2: suite toda** — `pio test -e native` + `pytest` verdes; build
esp32dev ok; flash final + smoke geral na bancada.
- [ ] **Step 3: Commit** — `chore: fw 0.4.0 — fase 2 completa`

## Self-review

- Cobertura da spec F2: server (Docker, .env.example, /copa/*, /health,
  auth) ✓; firmware (wifi, http, app_copa, alarme RTTTL) ✓; NTP→RTC
  (ajuste pós-spec: RTC entrou no projeto) ✓; fonte sem key validada ✓;
  CI ✓. `/talk` fica explicitamente pra F3.
- Sem placeholders de plano; código real nos tasks de lógica; tasks de UI
  referenciam padrões já existentes no repo (draw_list, softkey, ícones).
- Consistência de nomes: `CachedFetcher.get()`, `CopaService`,
  `copa_parse/copa_linha`, `wifi::level()`, `alarm::set/tick` usados
  uniformemente entre tasks.
