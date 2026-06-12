import numpy as np
import pytest

from app import stt


class SegFake:
    def __init__(self, text):
        self.text = text


class ModeloFake:
    def __init__(self):
        self.recebido = None
        self.lang = None

    def transcribe(self, amostras, language=None):
        self.recebido = amostras
        self.lang = language
        return [SegFake(" oi "), SegFake("tudo bem ")], None


def test_transcreve_pcm16_convertendo_pra_float(monkeypatch):
    fake = ModeloFake()
    monkeypatch.setattr(stt, "_modelo", fake)
    pcm = np.array([0, 16384, -16384], dtype=np.int16).tobytes()
    texto = stt.transcrever(pcm, "pt", {"stt": "local"})
    assert texto == "oi tudo bem"
    assert fake.lang == "pt"
    assert fake.recebido.dtype == np.float32
    assert abs(fake.recebido[1] - 0.5) < 1e-4


def test_backend_api_reservado(monkeypatch):
    monkeypatch.setattr(stt, "_modelo", ModeloFake())
    with pytest.raises(NotImplementedError):
        stt.transcrever(b"", "pt", {"stt": "api"})


def test_wav_monta_header_riff():
    pcm = b"\x01\x02\x03\x04"
    wav = stt._wav(pcm)
    assert wav[:4] == b"RIFF"
    assert wav[8:16] == b"WAVEfmt "
    assert len(wav) == 44 + len(pcm)
    assert wav[44:] == pcm
    # 16 kHz mono 16-bit no fmt chunk
    import struct
    canais, taxa = struct.unpack("<H", wav[22:24])[0], struct.unpack(
        "<I", wav[24:28])[0]
    assert (canais, taxa) == (1, 16000)


class RespFake:
    def __init__(self, texto):
        self._texto = texto

    def raise_for_status(self):
        pass

    def json(self):
        return {"text": self._texto}


def test_backend_groq_manda_wav_e_devolve_texto(monkeypatch):
    visto = {}

    def post_fake(url, headers=None, files=None, data=None, timeout=None):
        visto.update(url=url, headers=headers, files=files, data=data)
        return RespFake("  oi groq  ")

    monkeypatch.setattr(stt.httpx, "post", post_fake)
    pcm = b"\x00\x00" * 16
    cfg = {"stt": "groq", "stt_api_key": "gsk-teste"}
    assert stt.transcrever(pcm, "pt", cfg) == "oi groq"
    assert visto["url"] == stt.GROQ_URL
    assert visto["headers"]["Authorization"] == "Bearer gsk-teste"
    assert visto["data"] == {"model": stt.GROQ_MODELO, "language": "pt"}
    nome, wav, mime = visto["files"]["file"]
    assert wav[:4] == b"RIFF" and wav[44:] == pcm


def test_backend_groq_sem_lang_nao_manda_language(monkeypatch):
    visto = {}

    def post_fake(url, headers=None, files=None, data=None, timeout=None):
        visto.update(data=data)
        return RespFake("ok")

    monkeypatch.setattr(stt.httpx, "post", post_fake)
    stt.transcrever(b"", "", {"stt": "groq", "stt_api_key": "k"})
    assert visto["data"] == {"model": stt.GROQ_MODELO}


def test_backend_groq_nao_carrega_whisper(monkeypatch):
    # com groq, o modelo local nem é tocado (não pode ser importado)
    monkeypatch.setattr(stt, "_carregar",
                        lambda: pytest.fail("carregou whisper a toa"))
    monkeypatch.setattr(stt.httpx, "post",
                        lambda *a, **kw: RespFake("certo"))
    assert stt.transcrever(b"", "pt", {"stt": "groq"}) == "certo"
