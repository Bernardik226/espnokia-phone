import httpx
import pytest

from app.fetcher import CachedFetcher

URL = "http://upstream.test/dados.json"


class RelogioFake:
    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


def monta(respostas, clock):
    """respostas: lista de status codes; cada request consome um."""
    chamadas = {"n": 0}

    def handler(request):
        status = respostas[min(chamadas["n"], len(respostas) - 1)]
        chamadas["n"] += 1
        return httpx.Response(status, json={"v": chamadas["n"]})

    transport = httpx.MockTransport(handler)
    f = CachedFetcher(URL, ttl_s=300, transport=transport, clock=clock)
    return f, chamadas


def test_usa_cache_dentro_do_ttl():
    clock = RelogioFake()
    f, chamadas = monta([200], clock)
    d1, idade1 = f.get()
    clock.t += 60
    d2, idade2 = f.get()
    assert chamadas["n"] == 1          # 2ª chamada não bateu na rede
    assert d1 == d2 == {"v": 1}
    assert idade1 == 0 and idade2 == 60


def test_refaz_apos_ttl():
    clock = RelogioFake()
    f, chamadas = monta([200, 200], clock)
    f.get()
    clock.t += 301                     # passou do TTL de 300 s
    d2, idade2 = f.get()
    assert chamadas["n"] == 2
    assert d2 == {"v": 2} and idade2 == 0


def test_erro_de_rede_serve_dado_antigo():
    clock = RelogioFake()
    f, chamadas = monta([200, 500], clock)
    f.get()
    clock.t += 400                     # TTL vencido, upstream agora dá 500
    d2, idade2 = f.get()
    assert chamadas["n"] == 2
    assert d2 == {"v": 1}              # dado antigo > nada
    assert idade2 == 400


def test_sem_cache_e_sem_rede_levanta():
    clock = RelogioFake()
    f, _ = monta([500], clock)
    with pytest.raises(Exception):
        f.get()
