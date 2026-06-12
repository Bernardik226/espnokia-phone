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
            return json.loads(arq.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            logging.warning("registro.json corrompido, recomeçando")
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
