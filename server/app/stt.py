"""Transcrição local com faster-whisper. O modelo (small, int8, CPU) só
carrega na primeira chamada: o boot do server continua leve e quem nunca
fala não paga a RAM. PCM16 LE mono 16 kHz entra, texto sai."""

MODELO = "small"

_modelo = None


def _carregar():
    global _modelo
    if _modelo is None:
        from faster_whisper import WhisperModel
        _modelo = WhisperModel(MODELO, device="cpu", compute_type="int8")
    return _modelo


def transcrever(pcm: bytes, lang: str, cfg: dict) -> str:
    if cfg.get("stt", "local") != "local":
        raise NotImplementedError("backend de STT por API é da fase do dashboard")
    import numpy as np
    amostras = np.frombuffer(pcm, dtype=np.int16).astype(np.float32) / 32768.0
    segmentos, _ = _carregar().transcribe(amostras, language=lang or None)
    return " ".join(s.text.strip() for s in segmentos).strip()
