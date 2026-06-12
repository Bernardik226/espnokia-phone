"""Voz do bichinho: PCM16 cru no corpo → faster-whisper → Claude → JSON
{"falei","resposta"}. Histórico por device fica em RAM (deque de 6 pares):
reiniciou o server, o pet esqueceu — bom o bastante pra um tamagotchi."""
import json
import logging
import time
from collections import deque

from app import config, stt

MAX_CORPO = 500 * 1024          # ~15,6 s de PCM16 16 kHz
MAX_TOKENS = 300
# preço do Haiku 4.5 em USD/token (1 USD in, 5 USD out por MTok);
# vira config quando o dashboard chegar
PRECO_IN = 1.00 / 1_000_000
PRECO_OUT = 5.00 / 1_000_000


def _chat_anthropic(cfg, system, mensagens):
    import anthropic
    cliente = anthropic.Anthropic(api_key=cfg["anthropic_api_key"])
    r = cliente.messages.create(model=cfg["claude_model"],
                                max_tokens=MAX_TOKENS,
                                system=system, messages=mensagens)
    return r.content[0].text, r.usage.input_tokens, r.usage.output_tokens


class VozService:
    def __init__(self, stt_fn=None, chat_fn=None, now_fn=time.time):
        self.stt_fn = stt_fn or stt.transcrever
        self.chat_fn = chat_fn          # None = Anthropic de verdade
        self.now_fn = now_fn
        self.historico = {}             # device -> deque (6 pares user/assistant)

    def responder(self, device: str, corpo: bytes, lang: str):
        if len(corpo) > MAX_CORPO:
            return 413, {"erro": "audio grande demais"}
        cfg = config.load()
        try:
            falei = self.stt_fn(corpo, lang, cfg)
        except NotImplementedError:
            return 501, {"erro": "stt por api ainda nao existe"}
        if not falei:
            return 422, {"erro": "nao entendi"}
        if self.chat_fn is None and not cfg.get("anthropic_api_key"):
            return 502, {"erro": "sem chave anthropic no config"}
        hist = self.historico.setdefault(device, deque(maxlen=12))
        system = (f"{cfg['persona']} Responda em ate "
                  f"{cfg['max_resposta_chars']} caracteres, no idioma "
                  f"{lang or 'pt'}, sem markdown.")
        mensagens = list(hist) + [{"role": "user", "content": falei}]
        try:
            resposta, t_in, t_out = (self.chat_fn or _chat_anthropic)(
                cfg, system, mensagens)
        except Exception:
            logging.exception("claude api falhou")
            return 502, {"erro": "sem resposta do claude"}
        hist.append({"role": "user", "content": falei})
        hist.append({"role": "assistant", "content": resposta})
        try:
            self._loga_uso(device, t_in, t_out)
        except OSError:
            logging.warning("falha ao gravar uso.jsonl", exc_info=True)
        return 200, {"falei": falei, "resposta": resposta}

    def _loga_uso(self, device, t_in, t_out):
        linha = {"ts": int(self.now_fn()), "device": device,
                 "tokens_in": t_in, "tokens_out": t_out,
                 "custo_usd": round(t_in * PRECO_IN + t_out * PRECO_OUT, 6)}
        with open(config.data_dir() / "uso.jsonl", "a") as f:
            f.write(json.dumps(linha, ensure_ascii=False) + "\n")
