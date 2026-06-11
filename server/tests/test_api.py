import json
import pathlib

from fastapi.testclient import TestClient

from app.copa import CopaService
from app.main import create_app

FIX = pathlib.Path(__file__).parent / "fixtures" / "worldcup_sample.json"

ANTES_DA_COPA = 1_781_000_000        # 2026-06-09, nenhum jogo começou
ABERTURA_ROLANDO = 1_781_204_400 + 600   # 10 min depois de MEX x RSA


class FetcherFixture:
    def get(self):
        return json.loads(FIX.read_text()), 42


class LiveScoresFake:
    def __init__(self, tabela=None):
        self.tabela = tabela or {}

    def scores(self):
        return self.tabela


def monta(device_keys="", agora=ANTES_DA_COPA, placares=None):
    svc = CopaService(FetcherFixture(), now_fn=lambda: agora)
    app = create_app(copa_service=svc, live_scores=LiveScoresFake(placares),
                     device_keys=device_keys)
    return TestClient(app)


def test_401_sem_chave():
    c = monta(device_keys="segredo")
    assert c.get("/copa/proximos").status_code == 401


def test_200_com_chave():
    c = monta(device_keys="segredo,outra")
    r = c.get("/copa/proximos", headers={"X-Device-Key": "outra"})
    assert r.status_code == 200


def test_health_sempre_aberto():
    c = monta(device_keys="segredo")
    assert c.get("/health").status_code == 200


def test_sem_keys_auth_desligada():
    c = monta(device_keys="")
    assert c.get("/copa/proximos").status_code == 200


def test_shape_do_contrato():
    c = monta()
    body = c.get("/copa/proximos?n=8").json()
    assert body["atualizado_s"] == 42
    j = body["jogos"][0]                       # abertura MEX x RSA, 16:00 BRT
    assert j == {"dia": 11, "mes": 6, "h": 16, "m": 0, "t1": "MEX", "t2": "RSA",
                 "info": "Grupo A", "s1": -1, "s2": -1, "live": False}


def test_n_limita_resposta():
    c = monta()
    assert len(c.get("/copa/proximos?n=2").json()["jogos"]) == 2


def test_brasil_so_jogos_do_brasil():
    c = monta()
    jogos = c.get("/copa/brasil").json()["jogos"]
    assert jogos and all("BRA" in (j["t1"], j["t2"]) for j in jogos)


def test_live_enriquece_placar():
    c = monta(agora=ABERTURA_ROLANDO, placares={("MEX", "RSA"): (1, 0)})
    jogos = c.get("/copa/live").json()["jogos"]
    assert len(jogos) == 1
    assert jogos[0]["t1"] == "MEX" and jogos[0]["live"] is True
    assert (jogos[0]["s1"], jogos[0]["s2"]) == (1, 0)
