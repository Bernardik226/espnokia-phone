"""Transcrição plugável. Default "local" (faster-whisper small, int8, CPU):
funciona out-of-the-box e o modelo só carrega na primeira chamada — quem
nunca fala não paga a RAM (STT_PRELOAD=1 aquece no boot). Backend "groq"
manda o áudio pra API da Groq (whisper-large-v3-turbo): transcrição melhor
e em fração de segundo, sem pesar a CPU do server — basta a chave no
config. PCM16 LE mono 16 kHz entra, texto sai."""
import struct

import httpx

MODELO = "small"
GROQ_URL = "https://api.groq.com/openai/v1/audio/transcriptions"
GROQ_MODELO = "whisper-large-v3-turbo"

_modelo = None


def _carregar():
    global _modelo
    if _modelo is None:
        from faster_whisper import WhisperModel
        _modelo = WhisperModel(MODELO, device="cpu", compute_type="int8")
    return _modelo


def _wav(pcm: bytes, taxa: int = 16000) -> bytes:
    """Header WAV PCM16 mono + amostras: o container que as APIs esperam."""
    return (b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVEfmt " +
            struct.pack("<IHHIIHH", 16, 1, 1, taxa, taxa * 2, 2, 16) +
            b"data" + struct.pack("<I", len(pcm)) + pcm)


def _groq(pcm: bytes, lang: str, cfg: dict) -> str:
    dados = {"model": GROQ_MODELO}
    if lang:
        dados["language"] = lang
    r = httpx.post(GROQ_URL,
                   headers={"Authorization":
                            f"Bearer {cfg.get('stt_api_key', '')}"},
                   files={"file": ("voz.wav", _wav(pcm), "audio/wav")},
                   data=dados, timeout=20.0)
    r.raise_for_status()
    return r.json().get("text", "").strip()


def _local(pcm: bytes, lang: str) -> str:
    import numpy as np
    amostras = np.frombuffer(pcm, dtype=np.int16).astype(np.float32) / 32768.0
    segmentos, _ = _carregar().transcribe(amostras, language=lang or None)
    return " ".join(s.text.strip() for s in segmentos).strip()


def transcrever(pcm: bytes, lang: str, cfg: dict) -> str:
    backend = cfg.get("stt", "local")
    if backend == "groq":
        return _groq(pcm, lang, cfg)
    if backend != "local":
        raise NotImplementedError(f"backend de STT desconhecido: {backend}")
    return _local(pcm, lang)
