"""Config local dashboard-ready: config.json em DATA_DIR, semeado dos envs
na primeira criação e relido a cada request (arquivo pequeno — o dashboard
futuro edita e vale na hora, sem restart)."""
import json
import os
from pathlib import Path

PERSONA = (
    "Você é o Claude, um bichinho virtual que mora num celular Nokia dos "
    "anos 2000. Você é carinhoso, curioso e meio bobo. Responda curto, sem "
    "emojis e sem markdown: a telinha só mostra texto puro."
)

DEFAULTS = {
    "anthropic_api_key": "",
    "claude_model": "claude-haiku-4-5-20251001",
    "stt": "local",          # ou "groq" (whisper na API da Groq, mais fiel)
    "stt_api_key": "",       # chave da Groq quando stt = "groq"
    "persona": PERSONA,
    "max_resposta_chars": 220,
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
