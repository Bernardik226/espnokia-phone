"""Voz do bichinho: PCM16 cru no corpo → STT plugável → Claude → JSON
{"falei","resposta"}. Histórico por device persiste em DATA_DIR/claw/ via
MemoriaService: o pet lembra das últimas conversas e carrega a memória
resumida no system — reiniciou o server, ele continua o papo."""
import json
import logging
import time

from app import config, stt
from app.memoria import MAX_R_BYTES, MemoriaService, corta_utf8

MAX_CORPO = 500 * 1024          # ~15,6 s de PCM16 16 kHz
MAX_TOKENS = 300


def _chat_anthropic(cfg, system, mensagens):
    import anthropic
    cliente = anthropic.Anthropic(api_key=cfg["anthropic_api_key"])
    tools = []
    if cfg.get("web_search", True):
        # busca server-side da API: no maximo 1 por fala (~US$0,01 quando
        # o Claude decide usar; perguntas comuns nao buscam e nao pagam)
        tools = [{"type": "web_search_20250305", "name": "web_search",
                  "max_uses": 1}]
    r = cliente.messages.create(model=cfg["claude_model"],
                                max_tokens=MAX_TOKENS,
                                system=system, messages=mensagens,
                                tools=tools)
    # com busca, o content mistura blocos de tool use: o texto é o que vale
    texto = " ".join(b.text.strip() for b in r.content
                     if b.type == "text").strip()
    buscas = getattr(getattr(r.usage, "server_tool_use", None),
                     "web_search_requests", 0) or 0
    return texto, r.usage.input_tokens, r.usage.output_tokens, buscas


class VozService:
    def __init__(self, stt_fn=None, chat_fn=None, now_fn=time.time,
                 memoria=None):
        self.stt_fn = stt_fn or stt.transcrever
        self.chat_fn = chat_fn          # None = Anthropic de verdade
        self.now_fn = now_fn
        self.memoria = memoria or MemoriaService(now_fn=now_fn)

    def responder(self, device: str, corpo: bytes, lang: str, t: str = ""):
        if len(corpo) > MAX_CORPO:
            return 413, {"erro": "audio grande demais"}
        cfg = config.load()
        try:
            falei = self.stt_fn(corpo, lang, cfg)
        except NotImplementedError:
            return 501, {"erro": "backend de stt desconhecido"}
        except Exception:
            logging.exception("stt falhou")
            return 502, {"erro": "stt falhou"}
        if not falei:
            return 422, {"erro": "nao entendi"}
        if self.chat_fn is None and not cfg.get("anthropic_api_key"):
            return 502, {"erro": "sem chave anthropic no config"}
        system = (f"{config.persona_prompt(cfg)} Responda em ate "
                  f"{cfg['max_resposta_chars']} caracteres, no idioma "
                  f"{lang or 'pt'}, sem markdown.")
        if t:  # hora local mandada pelo device (RTC): o pet sabe que horas sao
            system += f" Agora, no relógio do usuário, são {t}."
        if cfg.get("web_search", True):
            system += (" Você pode buscar na internet quando precisar de "
                       "informação atual. Nunca invente placar, notícia ou "
                       "fato recente: ou você busca, ou diz que não sabe. "
                       "Responda só o fato, sem citar links nem fontes.")
        mem = self.memoria.memoria_texto(device)
        if mem:
            system += f"\n\nSuas memórias das conversas passadas:\n{mem}"
        mensagens = (self.memoria.recentes(device) +
                     [{"role": "user", "content": falei}])
        try:
            res = (self.chat_fn or _chat_anthropic)(cfg, system, mensagens)
        except Exception:
            logging.exception("claude api falhou")
            return 502, {"erro": "sem resposta do claude"}
        resposta, t_in, t_out = res[0], res[1], res[2]
        buscas = res[3] if len(res) > 3 else 0
        # corta a resposta no mesmo teto do registro (MAX_R_BYTES, UTF-8-safe):
        # a fala em tempo real e o histórico mostram EXATAMENTE o mesmo texto
        resposta = corta_utf8(resposta, MAX_R_BYTES)
        self.memoria.grava_par(device, falei, resposta, t)
        try:
            self._loga_uso(device, t_in, t_out, buscas, cfg["claude_model"],
                           len(corpo), cfg["stt"])
        except OSError:
            logging.warning("falha ao gravar uso.jsonl", exc_info=True)
        return 200, {"falei": falei, "resposta": resposta}

    def _loga_uso(self, device, t_in, t_out, buscas, model, corpo_bytes, stt):
        pin, pout = config.preco_de(model)
        custo = round(t_in * pin + t_out * pout + buscas * config.PRECO_BUSCA, 6)
        segundos = round(corpo_bytes / 32000, 3)   # PCM16 16kHz mono = 32000 B/s
        # STT: Groq cobra por segundo de áudio; whisper local é grátis
        custo_stt = round(segundos * config.PRECO_STT_SEG, 6) if stt == "groq" else 0.0
        linha = {"ts": int(self.now_fn()), "device": config.id8(device),
                 "tokens_in": t_in, "tokens_out": t_out, "buscas": buscas,
                 "custo_usd": custo, "segundos": segundos, "custo_stt": custo_stt}
        with open(config.data_dir() / "uso.jsonl", "a") as f:
            f.write(json.dumps(linha, ensure_ascii=False) + "\n")
