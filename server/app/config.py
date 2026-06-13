"""Config local dashboard-ready: config.json em DATA_DIR, semeado dos envs
na primeira criação e relido a cada request (arquivo pequeno — o dashboard
futuro edita e vale na hora, sem restart)."""
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


def persona_prompt(cfg: dict) -> str:
    """System da personalidade ativa (o combobox guarda só o id dela)."""
    p = PERSONAS.get(cfg.get("persona_id", PERSONA_DEFAULT),
                     PERSONAS[PERSONA_DEFAULT])
    return PERSONA_BASE.format(traco=p["traco"])


DEFAULTS = {
    "anthropic_api_key": "",
    "claude_model": "claude-haiku-4-5-20251001",
    "stt": "local",          # ou "groq" (whisper na API da Groq, mais fiel)
    "stt_api_key": "",       # chave da Groq quando stt = "groq"
    "persona_id": PERSONA_DEFAULT,   # qual personalidade do PERSONAS está ativa
    "max_resposta_chars": 220,
    "web_search": True,      # o pet pode dar 1 busca na internet por fala
}


def data_dir() -> Path:
    d = Path(os.environ.get("DATA_DIR", "./data"))
    d.mkdir(parents=True, exist_ok=True)
    return d


def load() -> dict:
    arq = data_dir() / "config.json"
    if not arq.exists():
        cfg = dict(DEFAULTS)
        cfg["anthropic_api_key"] = os.environ.get("ANTHROPIC_API_KEY", "")
        cfg["stt_api_key"] = os.environ.get("GROQ_API_KEY", "")
        # com chave da Groq no env, ela vira o default (STT_BACKEND força)
        cfg["stt"] = os.environ.get(
            "STT_BACKEND", "groq" if cfg["stt_api_key"] else "local")
        arq.write_text(json.dumps(cfg, indent=2, ensure_ascii=False), encoding="utf-8")
        return cfg
    cfg = dict(DEFAULTS)
    cfg.update(json.loads(arq.read_text(encoding="utf-8")))
    return cfg


def save(parcial: dict) -> dict:
    """Funde campos conhecidos no config.json e devolve o config novo. O
    dashboard edita campo a campo e vale na hora (load relê a cada request).
    Chaves de fora de DEFAULTS são ignoradas — nada de gravar lixo do form."""
    cfg = load()
    cfg.update({k: v for k, v in parcial.items() if k in DEFAULTS})
    arq = data_dir() / "config.json"
    tmp = arq.with_suffix(".tmp")        # escrita atômica
    tmp.write_text(json.dumps(cfg, indent=2, ensure_ascii=False),
                   encoding="utf-8")
    os.replace(tmp, arq)
    return cfg
