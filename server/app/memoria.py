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
    "1500 caracteres, markdown simples (use títulos e listas se ajudar), "
    "guardando o que importa: fatos sobre quem fala com você, gostos, "
    "piadas internas, o clima das conversas e suas percepções. Funda a "
    "memória antiga com o novo sem perder o que ainda importa. Responda "
    "SÓ com a memória nova."
)


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
            conversas=conversas, lang=lang or "pt")
        system = cfg["persona"] + (" Você vai atualizar suas memórias "
                                   "das conversas.")
        nova = (self.chat_fn or _chat_anthropic)(
            cfg, system, [{"role": "user", "content": user}])
        nova = nova[:MAX_MEMORIA_CHARS]
        (self._dir(device) / "memoria.md").write_text(nova, encoding="utf-8")
        reg["pares"] = reg["pares"][len(antigos):]
        reg["resumidos"] += len(antigos)
        self._salva(device, reg)
