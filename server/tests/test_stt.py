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
