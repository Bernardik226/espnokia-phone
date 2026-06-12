"""Memória do Claw'd: registro das conversas por device em DATA_DIR e
resumo acumulativo gerado pelo próprio Claude quando o registro enche.
data/claw/<id8>/registro.json + memoria.md — arquivos legíveis, de
propósito: dá pra abrir e ler o que o pet anda guardando."""
import hashlib
import json
import logging
import os
import time

from app import config

MAX_PARES = 30      # margem: atingiu, dispara o resumo
VARRE = 20          # pares antigos fundidos na memória e excluídos
TETO = 40           # resumo falhando: acima disso descarta o mais antigo
PARES_POR_PAG = 6   # paginação do GET /claude/registro
MAX_Q_BYTES = 160   # transcrição truncada no endpoint (buffers do ESP32)
MAX_R_BYTES = 280   # resposta truncada no endpoint
MAX_MEMORIA_CHARS = 1500
MAX_TOKENS_RESUMO = 600

PROMPT_RESUMO = (
    "Minha memória atual:\n{memoria}\n\n"
    "Conversas novas pra incorporar:\n{conversas}\n"
    "Reescreva sua memória em primeira pessoa, no idioma {lang}, em até "
    "{max_chars} caracteres, markdown simples (use títulos e listas se ajudar), "
    "guardando o que importa: fatos sobre quem fala com você, gostos, "
    "piadas internas, o clima das conversas e suas percepções. Funda a "
    "memória antiga com o novo sem perder o que ainda importa. Responda "
    "SÓ com a memória nova."
)


def _corta_utf8(s: str, max_bytes: int) -> str:
    """Trunca em bytes sem quebrar caractere UTF-8 (o ESP32 dimensiona
    buffers em bytes)."""
    b = s.encode("utf-8")
    if len(b) <= max_bytes:
        return s
    corte = b[:max_bytes - 3]      # espaço pro "…" (3 bytes)
    while corte:
        try:                       # descarta o caractere partido no fim
            return corte.decode("utf-8") + "…"
        except UnicodeDecodeError:
            corte = corte[:-1]
    return "…"


def _chat_anthropic(cfg, system, mensagens):
    import anthropic
    cliente = anthropic.Anthropic(api_key=cfg["anthropic_api_key"])
    r = cliente.messages.create(model=cfg["claude_model"],
                                max_tokens=MAX_TOKENS_RESUMO,
                                system=system, messages=mensagens)
    return r.content[0].text


class MemoriaService:
    def __init__(self, base_dir=None, chat_fn=None, now_fn=time.time):
        self._base = base_dir           # None = config.data_dir() na hora
        self.chat_fn = chat_fn          # None = Anthropic de verdade
        self.now_fn = now_fn

    def _dir(self, device):
        base = self._base or config.data_dir()
        id8 = hashlib.sha256(device.encode()).hexdigest()[:8]
        d = base / "claw" / id8         # a chave do device é secreta:
        d.mkdir(parents=True, exist_ok=True)   # só o hash vira pasta
        return d

    def _carrega(self, device):
        arq = self._dir(device) / "registro.json"
        if not arq.exists():
            return {"pares": [], "resumidos": 0}
        try:
            reg = json.loads(arq.read_text(encoding="utf-8"))
            # valida a forma: deve ser dict com pares=lista e resumidos=int
            if (not isinstance(reg, dict) or
                not isinstance(reg.get("pares"), list) or
                not isinstance(reg.get("resumidos"), int)):
                raise ValueError("formato inválido")
            return reg
        except (json.JSONDecodeError, OSError, ValueError):
            logging.warning("registro.json corrompido em %s, recomeçando", arq)
            return {"pares": [], "resumidos": 0}

    def _salva(self, device, reg):
        arq = self._dir(device) / "registro.json"
        tmp = arq.with_suffix(".tmp")   # escrita atômica
        tmp.write_text(json.dumps(reg, ensure_ascii=False), encoding="utf-8")
        os.replace(tmp, arq)

    def grava_par(self, device, q, r):
        reg = self._carrega(device)
        reg["pares"].append({"ts": int(self.now_fn()), "q": q, "r": r})
        if len(reg["pares"]) > TETO:    # resumo vive falhando: não cresce
            reg["pares"] = reg["pares"][-TETO:]   # pra sempre
        self._salva(device, reg)

    def precisa_resumo(self, device):
        return len(self._carrega(device)["pares"]) >= MAX_PARES

    def memoria_texto(self, device):
        arq = self._dir(device) / "memoria.md"
        return arq.read_text(encoding="utf-8") if arq.exists() else ""

    def resumir(self, device, lang, cfg):
        reg = self._carrega(device)
        antigos = reg["pares"][:VARRE]
        if not antigos:
            return
        conversas = "".join(f"EU: {p['q']}\nVOCÊ: {p['r']}\n"
                            for p in antigos)
        user = PROMPT_RESUMO.format(
            memoria=self.memoria_texto(device) or "(ainda vazia)",
            conversas=conversas, lang=lang or "pt",
            max_chars=MAX_MEMORIA_CHARS)
        system = cfg["persona"] + (" Você vai atualizar suas memórias "
                                   "das conversas.")
        nova = (self.chat_fn or _chat_anthropic)(
            cfg, system, [{"role": "user", "content": user}])
        nova = nova[:MAX_MEMORIA_CHARS]
        arq = self._dir(device) / "memoria.md"
        tmp = arq.with_suffix(".tmp")   # escrita atômica
        tmp.write_text(nova, encoding="utf-8")
        os.replace(tmp, arq)
        # recarrega antes de varrer: um grava_par pode ter chegado
        # enquanto a API pensava (resumo roda em background)
        vistos = {(p["ts"], p["q"]) for p in antigos}
        reg = self._carrega(device)
        reg["pares"] = [p for p in reg["pares"]
                        if (p["ts"], p["q"]) not in vistos]
        reg["resumidos"] += len(antigos)
        self._salva(device, reg)

    def pares(self, device, pag):
        todos = list(reversed(self._carrega(device)["pares"]))
        total = len(todos)
        pags = (total + PARES_POR_PAG - 1) // PARES_POR_PAG
        ini = pag * PARES_POR_PAG
        # d/h prontos pro display (fuso = TZ do ambiente do server)
        itens = [{"q": _corta_utf8(p["q"], MAX_Q_BYTES),
                  "r": _corta_utf8(p["r"], MAX_R_BYTES),
                  "d": time.strftime("%d/%m", time.localtime(p["ts"])),
                  "h": time.strftime("%H:%M", time.localtime(p["ts"]))}
                 for p in todos[ini:ini + PARES_POR_PAG]]
        return {"total": total, "pags": pags, "pag": pag, "itens": itens}

    def memoria(self, device):
        arq = self._dir(device) / "memoria.md"
        if not arq.exists():
            return {"memoria": "", "resumidos": 0, "ts": 0}
        return {"memoria": arq.read_text(encoding="utf-8"),
                "resumidos": self._carrega(device)["resumidos"],
                "ts": int(arq.stat().st_mtime)}

    def recentes(self, device, n=6):
        msgs = []
        for p in self._carrega(device)["pares"][-n:]:
            msgs.append({"role": "user", "content": p["q"]})
            msgs.append({"role": "assistant", "content": p["r"]})
        return msgs
