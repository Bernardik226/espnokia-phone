# Arquitetura & decisões técnicas — espnokia-phone

Este documento explica **como o projeto está montado por dentro** e as decisões que tomei pra fazer um handheld estilo Nokia 3310 rodar um assistente de voz com o Claude — mantendo histórico de conversa — num **ESP32 comum, sem PSRAM**. Se você quer a visão de vitrine (o que o aparelho faz), volte pro [README](../README.pt-br.md); aqui é a parte de engenharia.

O fio condutor é um só: **cliente magro no firmware, server gordo no backend.** O chip guarda o mínimo e pagina o resto; o server segura o estado durável e resume o que fica grande demais.

---

## 1. As 3 partes + contrato de API

| Parte | Stack | Papel |
|---|---|---|
| **Firmware** | ESP32 (C++17 / Arduino / U8g2, PlatformIO) | Cliente magro: UI, captura de voz, paginação sob demanda |
| **Server** | FastAPI / Uvicorn (Docker) | Server gordo: transcrição, chamadas ao Claude, memória durável, mastiga fontes de dados |
| **PWA** | HTML/JS de página única (sem framework) | Configuração do aparelho: pareamento, status, chaves, persona |

O firmware **nunca** toca uma fonte externa direto — ele só fala com o *meu* server, que é a membrana entre um buffer de poucos KB e a internet aberta. Toda rota (menos o essencial de saúde/PWA) exige o header **`X-Device-Key`**.

### Autenticação — `X-Device-Key` (`server/app/auth.py`)

A dependency `make_auth` valida o header em 3 modos, pela env `DEVICE_KEYS`:

- **`""`** → auth desligada (dev local).
- **`"*"`** → modo **capability**: aceita qualquer chave com `len ≥ 16`. A própria chave forte (32 hex = 128 bits, gerada no device) *é* a credencial; não há lista de usuários no servidor, e os dados ficam **isolados por hash da chave**. É o modo do pareamento por QR — um aparelho se registra sem cadastro central.
- **`"a,b"`** → allowlist clássica de chaves.

O dashboard e o device usam **a mesma auth**: o painel web é literalmente o mesmo "device" logado com a chave.

### Rotas

**Device-facing** (chamadas pelo firmware ESP32):

| Método | Rota | O que faz |
|---|---|---|
| `GET` | `/health` | Ping de keep-alive do socket — **sem auth**. |
| `GET` | `/copa/proximos?n=8` · `/copa/brasil` · `/copa/live` · `/copa/grupos` | Placares/tabela da Copa 2026 (payload compactado, chaves de 1 letra pra caber no buffer do ESP32). |
| `GET` | `/futebol/ligas` · `/futebol/jogos?liga=` · `/futebol/live?liga=` · `/futebol/tabela?liga=` | Futebol de clubes (ligas, jogos, ao vivo, classificação). |
| `POST` | `/claude/voz?lang=pt&t=` | **Rota central da voz.** Corpo = PCM16 cru; dispara STT → Claude → resposta; se o histórico encheu, agenda o resumo em background. |
| `GET` | `/claude/registro?pag=0` | Histórico paginado (6 pares por página). |
| `GET` | `/claude/memoria` | Memória resumida (markdown) + contador de resumos. |
| `POST` | `/claude/memoria/limpar` | Apaga registro + memória ("esquecer tudo"). |

**PWA / admin-facing** (dashboard):

| Método | Rota | O que faz |
|---|---|---|
| `GET` | `/admin/status` | Versão, motor de STT, modelo, web search, se tem API key, total de conversas, nº de resumos, timestamp da memória. |
| `GET` | `/admin/config` | Config atual: `persona_id` + catálogo de personas, modelo, STT, limite de resposta, web search, e **flags booleanas** `tem_anthropic_key` / `tem_stt_key` — **nunca a chave crua**. |
| `POST` | `/admin/config` | Grava campos parciais; a chave de API só é sobrescrita se vier não-vazia (evita apagar sem querer); `persona_id` só é aceito se existir no catálogo. |
| `GET` | `/manifest.json` · `/sw.js` · `/` | Shell do PWA (sem auth). |

---

## 2. A restrição-chave e como resolvi

**O problema:** rodar o Claude *com memória de conversa* num ESP32 WROOM-32 — sem PSRAM, com RAM e flash apertadas (por isso `board=esp32dev` + partição `huge_app.csv`: abri mão de OTA/SPIFFS pra ter espaço de código). Um histórico de chat cresce sem limite; um buffer de resposta multibyte estoura fácil; e transcrição de áudio no chip é impensável.

**A solução:** dividir responsabilidades. O device fica *magro*; o server fica *gordo*.

### Lado device — só o que cabe em RAM fixa

- **Buffers fixos, dimensionados em bytes** (`firmware/lib/claudemodel/claudemodel.h`). Nada de alocação dinâmica:
  ```cpp
  struct RegPar {
    char q[164];   // MAX_Q_BYTES do server (160) + nul
    char r[284];   // MAX_R_BYTES do server (280) + nul
    char d[6];     // "dd/mm"
    char h[6];     // "hh:mm"
  };
  ```
  Os tamanhos **164 / 284 espelham literalmente** os tetos do server (`MAX_Q_BYTES` / `MAX_R_BYTES` + 1): o firmware não decide o teto sozinho, ele reflete o do servidor. `registro_parse(RegPar* itens, uint8_t max)` recebe do *caller* quantos pares cabem — quem manda na RAM é o app, não a lib.
- **Máquina de estados push-to-talk** (testável no PC, sem Arduino):
  ```
  E_PET → (OK aperta) → E_OUVINDO → (OK solta) → E_PENSANDO → (resposta|falha) → E_FALANDO → (volta) → E_PET
  ```
  Com timeouts de segurança: `kOuvindoMaxMs = 15 s` corta a gravação; `kFalandoMaxMs = 60 s` volta sozinho ao pet. O `E_PENSANDO` **não** tem timeout próprio de propósito — quem corta é o timeout do cliente HTTP (`kRespTimeoutMs = 20 s`).
- **Paginação, não replicação.** O histórico completo **não existe no device**: `GET /claude/registro?pag=N` devolve só uma página (6 pares), e o firmware guarda no máximo o array da página atual. Entre reboots o device persiste só **a chave e a URL do server** (em NVS) — nenhuma conversa fica na flash.
- **Consciência de heap explícita.** O upload de voz vai por **HTTP chunked**: o PCM16 é escrito direto no socket TLS conforme a captura, chunk a chunk, sem montar o corpo inteiro em RAM. O socket TLS é mantido "quente" e reusado entre chamadas, e `ESP.getFreeHeap()` é logado em cada tentativa de rede. (O TLS usa `setInsecure()` de propósito — "o segredo do device é a chave, não o canal"; o ESP32 não carrega um bundle de CAs atualizado.)

### Lado server — o estado durável e o resumo (`server/app/memoria.py`)

Esse arquivo é o coração da solução. Por device, guardo em disco (`DATA_DIR/claw/<id8>/`, onde `id8 = sha256(chave)[:8]` — a chave secreta nunca vira nome de pasta):

- **`registro.json`** — `{"pares": [...], "resumidos": int}`; cada par é `{ts, q, r}`. Escrita **atômica** (`tmp` + `os.replace`).
- **`memoria.md`** — texto livre em markdown, um resumo cumulativo em 1ª pessoa que o **próprio Claude** escreve. Legível de propósito.

Os limites que governam tudo:

```python
MAX_PARES = 30           # atingiu -> dispara resumo
VARRE     = 20           # pares mais antigos fundidos na memória e removidos
TETO      = 40           # rede de segurança: acima disso descarta o mais antigo
PARES_POR_PAG = 6        # paginação do GET /claude/registro
MAX_Q_BYTES = 160        # pergunta truncada (buffer do ESP32)
MAX_R_BYTES = 280        # resposta truncada (buffer do ESP32)
MAX_MEMORIA_CHARS = 1500 # teto do memoria.md
```

**O mecanismo de resumo (contexto limitado):**

1. Depois de cada `/claude/voz` bem-sucedido, `precisa_resumo()` checa se `len(pares) ≥ 30`. Se sim, agenda `resumir()` num **`BackgroundTasks`** — não bloqueia a resposta ao aparelho, e se falhar só loga.
2. `resumir()` pega os **20 pares mais antigos** + o `memoria.md` atual e pede pro Claude **reescrever a memória inteira** em 1ª pessoa, no idioma do usuário, em até **1500 caracteres**. Ou seja: cada resumo **funde** o velho com o novo e é resumido de novo — não é um log incremental, o tamanho fica sempre controlado.
3. O resultado é truncado e salvo atomicamente; os 20 pares resumidos são removidos do `registro.json` — mas o registro é **recarregado do disco antes de filtrar**, pra não perder um par que chegou enquanto o resumo (em background) pensava.
4. **Rede de segurança:** se o resumo nunca rodar (sem API key, falha contínua), `grava_par` corta o array pros últimos **40 pares** a cada gravação — um cap duro que impede o `registro.json` de crescer sem limite, ao custo de descartar silenciosamente os mais antigos ainda não resumidos.

O efeito é o que importa: **o contexto que mando pro Claude a cada fala nunca é o histórico completo.** É `memoria.md` (≤ 1500 chars) + só os **últimos 6 pares** crus + a fala atual. O histórico completo em disco existe só pra exibição paginada no aparelho/dashboard — jamais é reenviado inteiro ao modelo.

### `corta_utf8` — a ponte de segurança de bytes entre Python e C

O truncamento é feito **em bytes, não em caracteres Python**, porque o firmware aloca `char q[164]` / `char r[284]` fixos. Cortar por caractere estouraria o buffer C num texto multibyte (acentos, emoji):

```python
def corta_utf8(s: str, max_bytes: int) -> str:
    b = s.encode("utf-8")
    if len(b) <= max_bytes:
        return s
    corte = b[:max_bytes - 3]          # reserva 3 bytes pro "…"
    while corte:
        try:
            return corte.decode("utf-8") + "…"
        except UnicodeDecodeError:
            corte = corte[:-1]         # descarta byte de continuação partido
    return "…"
```

Uso o mesmo `corta_utf8` no corte ao vivo da resposta e na paginação do histórico — assim a fala "ao vivo" e o registro mostram **exatamente o mesmo texto**, e o firmware nunca recebe mais bytes do que o buffer aguenta.

### Footprint

Com o NokiaOS inteiro, os apps, WiFi/TLS e o pipeline de voz, o firmware ocupa **flash 36.1% / RAM 25.6%** — cerca de um terço da flash e um quarto da RAM. Sobra folga de propósito: cada nova tela é uma struct de callbacks, não uma reforma de memória.

<p align="center">
  <img src="assets/footprint.png" width="640" alt="footprint do firmware na partição huge_app: flash ~36% usada, RAM ~26% usada">
</p>

---

## 3. Transcrição (STT)

A transcrição é **plugável** via `cfg["stt"]` (default `"local"`), trocável pelo dashboard sem reflash:

- **`local`** (padrão) — **faster-whisper `small`, quantização int8, na CPU**. É **lazy-load**: o modelo só carrega na primeira fala, pra não atrasar o boot; com `STT_PRELOAD=1` eu aqueço no boot quando quero latência baixa já na primeira interação. Roda sem depender de rede nem de chave externa.
- **`groq`** (opcional) — `POST` pra `api.groq.com/.../audio/transcriptions`, modelo **`whisper-large-v3-turbo`** (mais rápido/melhor, mas precisa de chave e rede).

### O fluxo de uma fala (`server/app/claude_voz.py`)

```
PCM16 16 kHz cru no corpo (MAX_CORPO = 500 KB ≈ 15,6 s)
   → transcrever()                       # STT local ou Groq
   → _chat_anthropic()                   # Claude Messages API
        · modelo vindo do cfg (classe Haiku, por custo/latência)
        · max_tokens = 300               # cabe na tela e segura o custo
        · web search server-side, max_uses = 1   # ≤ 1 busca por fala
   → JSON  { resposta, humor, ... }       # texto já cortado no orçamento de bytes
```

O ponto que amarra tudo à seção anterior: **a memória resumida (`memoria.md`) entra no `system` prompt** do Claude a cada fala. É isso que dá continuidade — o pet lembra de você **mesmo depois de reiniciar o server**, sem nunca carregar o histórico cru inteiro no contexto. A resposta já sai truncada pelo mesmo teto de bytes que o ESP32 pagina, então o que o Claude gera, o que a tela mostra e o que o histórico guarda são o mesmo texto.

---

## 4. O que a PWA configura

O PWA (`server/app/static/dashboard.html`) é um SPA de página única, sem framework, com i18n embutido (9 idiomas, troca em runtime persistida em `localStorage`). Ele não é firmware — é o **painel de configuração do aparelho**, instalável no celular. Descrevo aqui por **função** (a aparência será reformulada numa próxima rodada):

- **Pareamento por QR / descoberta.** O aparelho sorteia a própria chave no primeiro boot (RNG de hardware → NVS) e mostra um QR com o link `.../#k=<chave>`. Ao abrir, o JS lê a chave do fragmento, entra **já logado** e limpa a URL (`history.replaceState`) — a chave não fica visível na barra de endereço.
- **Login por chave.** Campo de chave testado contra `GET /admin/status`; 401 mostra "chave recusada", chave válida persiste em `localStorage`.
- **Status online.** Espelho read-only de `/admin/status`: versão, nº de conversas, nº de resumos, motor de STT, modelo, se a web search está ligada, se a chave Anthropic está configurada, quando a memória foi atualizada.
- **Configuração do Claw'd.** Escolher a **personalidade/persona** (catálogo em `config.PERSONAS`, ex.: Fofo, Sarcástico, Poeta, Sábio…), o **motor de STT** (local/Groq), o **modelo** do Claude, o **limite de resposta**, ligar/desligar **busca web**, e colar a **chave de API Anthropic** (campo senha — vazio = manter a atual). Grava via `POST /admin/config`.
- **Memória & conversas.** Ler o `memoria.md` cru, apagar tudo ("esquecer tudo" → `/claude/memoria/limpar`), e navegar as conversas paginadas (`/claude/registro`).

Não há gestão multi-device nem contas: **1 chave = 1 device = 1 sessão de configuração**.

---

## 5. Infra & organização

- **Dockerizado.** `server/Dockerfile` parte de `python:3.12-slim`, copia `requirements.txt` primeiro (cache de layer), expõe a 8000 e roda `uvicorn app.main:app`. Imagem simples, deploy único.
- **Pastas isoladas**, cada uma com seu próprio ciclo de build/test — sem tooling de monorepo compartilhado:
  ```
  firmware/   NokiaOS em C++/Arduino (PlatformIO: esp32dev + native)
    lib/      lógica pura testável: shell, btnlogic, rtttl, i18n, copamodel, claudemodel, snakegame...
    src/      drivers, apps, som, rede (wifi/http/ntp/provisionamento), conn
  server/     companion FastAPI: /copa /futebol /claude /admin + o dashboard PWA
  hardware/   case 3D / STL / configs de impressão
  docs/       instalação, arquitetura, assets
  tools/      utilitários de pixel art (grade → XBM)
  ```
- **Testes nativos.** `firmware/test/` roda no PC via PlatformIO Unity (ambiente `[env:native]`, sem placa): **99 casos** cobrindo shell, botões, RTTTL, formatação de hora, parse da Copa/futebol, o modelo do Claw'd, i18n e o Snake. `server/tests/` são **157 testes pytest** (fontes mockadas, auth, cache, personas, memória, endpoints). A separação `drivers/` (hardware) · `lib/` (lógica pura portável) · `apps/` (UI) é justamente o que permite testar no PC o que roda no ESP32.
- **Deploy — Railway.** O contrato de deploy é o `Dockerfile` plano (sem `railway.json`/`Procfile` versionados), compatível com Railway ou qualquer host de container. Envs relevantes: `DEVICE_KEYS` (allowlist / vazio / `*` capability), `ANTHROPIC_API_KEY` e `GROQ_API_KEY` (seed inicial, depois editáveis só pelo dashboard), `DATA_DIR`, `STT_BACKEND`, `STT_PRELOAD`, `LIVE_SOURCE_URL` (fonte reserva de placar ao vivo).

---

*Voltar pra vitrine → [README](../README.pt-br.md) · Montar o seu → [docs/INSTALL.pt-br.md](INSTALL.pt-br.md)*
