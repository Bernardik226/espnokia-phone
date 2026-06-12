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
    def __init__(self, tabela=None, grupos=None):
        self.tabela = tabela or {}
        self.tabela_grupos = grupos or []

    def jogos(self):
        return self.tabela

    def grupos(self):
        return self.tabela_grupos


def vivo(s1, s2, rolando=True, fim=False, minuto="", g1=None, g2=None, est=""):
    """Atalho pro shape que LiveScores.jogos() devolve por jogo."""
    return {"s1": s1, "s2": s2, "rolando": rolando, "fim": fim,
            "min": minuto, "g1": g1 or [], "g2": g2 or [], "est": est}


def monta(device_keys="", agora=ANTES_DA_COPA, placares=None, grupos=None):
    svc = CopaService(FetcherFixture(), now_fn=lambda: agora)
    app = create_app(copa_service=svc,
                     live_scores=LiveScoresFake(placares, grupos),
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
    c = monta(agora=ABERTURA_ROLANDO, placares={("MEX", "RSA"): vivo(1, 0)})
    jogos = c.get("/copa/live").json()["jogos"]
    assert len(jogos) == 1
    assert jogos[0]["t1"] == "MEX" and jogos[0]["live"] is True
    assert (jogos[0]["s1"], jogos[0]["s2"]) == (1, 0)


def test_live_leva_minuto_gols_e_estadio():
    c = monta(agora=ABERTURA_ROLANDO, placares={("MEX", "RSA"): vivo(
        2, 0, minuto="67", g1=["J. Quiñones 9'", "R. Jiménez 67'"],
        est="Estadio Azteca · Mexico City")})
    j = c.get("/copa/live").json()["jogos"][0]
    assert (j["s1"], j["s2"]) == (2, 0)
    assert j["min"] == "67"
    assert j["g1"] == "J. Quiñones 9'\nR. Jiménez 67'"
    assert "g2" not in j                     # sem gol → sem chave → payload enxuto
    assert j["est"] == "Estadio Azteca · Mexico City"


def test_estadio_aparece_em_jogo_futuro():
    c = monta(placares={("MEX", "RSA"): vivo(
        0, 0, rolando=False, est="Estadio Azteca · Mexico City")})
    j = c.get("/copa/proximos?n=1").json()["jogos"][0]
    assert j["est"] == "Estadio Azteca · Mexico City"
    assert "min" not in j and "g1" not in j
    assert (j["s1"], j["s2"]) == (-1, -1)    # não-live: placar da tabela intacto


def test_fim_na_janela_segura_placar_da_fonte():
    # Jogo acabou mas a tabela openfootball ainda não atualizou: dentro da
    # janela live, vale o placar final da fonte ao vivo.
    c = monta(agora=ABERTURA_ROLANDO,
              placares={("MEX", "RSA"): vivo(2, 0, rolando=False, fim=True)})
    j = c.get("/copa/live").json()["jogos"][0]
    assert (j["s1"], j["s2"]) == (2, 0)
    assert "min" not in j                    # acabou: minuto não faz sentido


def test_grupos_shape_do_contrato():
    c = monta(grupos=[{"n": "A", "t": [{"c": "MEX", "pts": 3, "j": 1, "sg": 2}]}])
    body = c.get("/copa/grupos").json()
    assert body == {"grupos": [{"n": "A",
                                "t": [{"c": "MEX", "p": 3, "j": 1, "s": 2}]}]}


def test_grupos_exige_chave():
    c = monta(device_keys="segredo")
    assert c.get("/copa/grupos").status_code == 401
