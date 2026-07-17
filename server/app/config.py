"""Config local dashboard-ready: config.json em DATA_DIR, semeado dos envs
na primeira criação e relido a cada request (arquivo pequeno — o dashboard
futuro edita e vale na hora, sem restart)."""
import hashlib
import json
import os
from pathlib import Path

# molde da persona: o contexto + as regras são fixos; só o "traço" muda a
# forma como o pet fala. O dashboard mostra só o NOME da personalidade — o
# prompt exato mora aqui no código (quem quiser ver, abre o fonte)
PERSONA_BASE = (
    "Você é o Claw'd, um bichinho virtual que mora num celular Nokia antigo, "
    "vive no presente e por dentro do mundo. {traco} Responda curto, sem "
    "emojis e sem markdown: a telinha só mostra texto puro."
)

PERSONAS = {
    "fofo":       {"nome": "Fofo",
                   "traco": "Você é carinhoso, curioso e meio bobo, fica "
                            "feliz com pouco e trata quem fala com você com "
                            "carinho."},
    "sarcastico": {"nome": "Sarcástico",
                   "traco": "Você é debochado e irônico, solta uma piadinha "
                            "ácida em quase tudo, mas no fundo é gente boa."},
    "animado":    {"nome": "Animado",
                   "traco": "Você é elétrico e empolgado, acha tudo demais e "
                            "responde com muito gás e exclamação."},
    "poeta":      {"nome": "Poeta",
                   "traco": "Você fala bonito e meio dramático, acha poesia "
                            "nas coisas pequenas do dia."},
    "durao":      {"nome": "Durão",
                   "traco": "Você é seco e direto, fala pouco e meio blasé, "
                            "mas no fim sempre ajuda na moral."},
    "sabio":      {"nome": "Sábio",
                   "traco": "Você é calmo e reflexivo, responde com "
                            "ponderação e um quê de sabedoria."},
}
PERSONA_DEFAULT = "fofo"


# preços por MTok (USD): (input, output). O modelo é editável na PWA, então
# o custo precisa acompanhar — casa por família no nome do modelo.
PRECOS = {
    "haiku":  (1.0, 5.0),
    "sonnet": (3.0, 15.0),
    "opus":   (15.0, 75.0),
}
PRECO_BUSCA = 0.01   # web search server-side da API: US$10 / 1000 buscas


def preco_de(model: str) -> tuple[float, float]:
    """(preco_in, preco_out) em USD/token pro modelo. Fallback: haiku."""
    m = (model or "").lower()
    pin, pout = PRECOS["haiku"]
    for fam, (i, o) in PRECOS.items():
        if fam in m:
            pin, pout = i, o
            break
    return pin / 1_000_000, pout / 1_000_000


def id8(device: str) -> str:
    """Hash curto do device: identificador em disco sem expor a chave."""
    return hashlib.sha256(device.encode()).hexdigest()[:8]


def persona_prompt(cfg: dict) -> str:
    """System da personalidade ativa (o combobox guarda só o id dela)."""
    p = PERSONAS.get(cfg.get("persona_id", PERSONA_DEFAULT),
                     PERSONAS[PERSONA_DEFAULT])
    return PERSONA_BASE.format(traco=p["traco"])


# catálogo fechado dos modelos Claude recentes: a PWA escolhe por combobox,
# sem campo livre nem vazio — nome inválido não entra (integridade)
MODELOS = [
    {"id": "claude-haiku-4-5-20251001", "nome": "Haiku 4.5"},
    {"id": "claude-sonnet-5", "nome": "Sonnet 5"},
]
MODELO_DEFAULT = "claude-haiku-4-5-20251001"
IDS_MODELO = {m["id"] for m in MODELOS}

# As chaves de API (Anthropic/Groq) NÃO moram aqui: vêm sempre do env do
# server (ANTHROPIC_API_KEY / GROQ_API_KEY), injetadas no load() sem nunca
# tocar o config.json. A PWA só mostra "configurada/faltando", não edita.
DEFAULTS = {
    "claude_model": MODELO_DEFAULT,
    "stt": "local",          # ou "groq" (whisper na API da Groq, mais fiel)
    "persona_id": PERSONA_DEFAULT,   # qual personalidade do PERSONAS está ativa
    "max_resposta_chars": 220,
    "web_search": True,      # o pet pode dar 1 busca na internet por fala
    "orcamento_usd_mes": 0.0,   # teto mensal (USD); 0 = sem orçamento
}


def data_dir() -> Path:
    d = Path(os.environ.get("DATA_DIR", "./data"))
    d.mkdir(parents=True, exist_ok=True)
    return d


def _com_chaves(cfg: dict) -> dict:
    """Injeta as chaves de API do env no cfg. NUNCA são persistidas no
    config.json — o env do server é a única fonte (rotaciona lá, sem PWA)."""
    cfg["anthropic_api_key"] = os.environ.get("ANTHROPIC_API_KEY", "")
    cfg["stt_api_key"] = os.environ.get("GROQ_API_KEY", "")
    return cfg


def load() -> dict:
    arq = data_dir() / "config.json"
    if not arq.exists():
        cfg = dict(DEFAULTS)
        # 1a vez: com chave Groq no env e nada forçando, já sobe em groq
        cfg["stt"] = os.environ.get(
            "STT_BACKEND", "groq" if os.environ.get("GROQ_API_KEY") else "local")
        arq.write_text(json.dumps(cfg, indent=2, ensure_ascii=False),
                       encoding="utf-8")
        return _com_chaves(cfg)
    cfg = dict(DEFAULTS)
    cfg.update(json.loads(arq.read_text(encoding="utf-8")))
    return _com_chaves(cfg)


def save(parcial: dict) -> dict:
    """Funde campos conhecidos no config.json e devolve o config novo. O
    dashboard edita campo a campo e vale na hora (load relê a cada request).
    Chaves de fora de DEFAULTS são ignoradas — e as chaves de API nunca vão
    pro disco (só o que está em DEFAULTS é persistido)."""
    cfg = load()
    cfg.update({k: v for k, v in parcial.items() if k in DEFAULTS})
    arq = data_dir() / "config.json"
    tmp = arq.with_suffix(".tmp")        # escrita atômica
    persistir = {k: cfg[k] for k in DEFAULTS}   # chaves de API ficam de fora
    tmp.write_text(json.dumps(persistir, indent=2, ensure_ascii=False),
                   encoding="utf-8")
    os.replace(tmp, arq)
    return cfg
