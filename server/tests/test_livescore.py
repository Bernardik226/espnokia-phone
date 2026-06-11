import httpx

from app.sources.livescore import LiveScores

BASE = "http://live.test"


class RelogioFake:
    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


def jogo(home, away, s1, s2, finished="FALSE", elapsed="45"):
    return {"home_team_name_en": home, "away_team_name_en": away,
            "home_score": str(s1), "away_score": str(s2),
            "finished": finished, "time_elapsed": elapsed}


def monta(payload=None, status=200, clock=None):
    chamadas = {"n": 0}

    def handler(request):
        chamadas["n"] += 1
        return httpx.Response(status, json=payload if payload is not None else {})

    ls = LiveScores(BASE, transport=httpx.MockTransport(handler),
                    clock=clock or RelogioFake())
    return ls, chamadas


def test_desligado_retorna_vazio():
    assert LiveScores("").scores() == {}
    assert LiveScores(None).scores() == {}


def test_upstream_500_retorna_vazio():
    ls, _ = monta(status=500)
    assert ls.scores() == {}


def test_payload_estranho_retorna_vazio():
    ls, _ = monta(payload={"surpresa": True})
    assert ls.scores() == {}


def test_em_andamento_vira_placar_com_codigo_fifa():
    ls, _ = monta(payload={"games": [jogo("Mexico", "South Africa", 1, 0)]})
    assert ls.scores() == {("MEX", "RSA"): (1, 0)}


def test_nao_comecado_e_finalizado_ficam_de_fora():
    ls, _ = monta(payload={"games": [
        jogo("Brazil", "Morocco", 0, 0, elapsed="notstarted"),
        jogo("Mexico", "South Africa", 2, 1, finished="TRUE"),
        jogo("France", "Senegal", 1, 1),
    ]})
    assert ls.scores() == {("FRA", "SEN"): (1, 1)}


def test_cache_de_60s_evita_rede():
    clock = RelogioFake()
    ls, chamadas = monta(payload={"games": [jogo("Brazil", "Haiti", 3, 0)]},
                         clock=clock)
    ls.scores()
    clock.t += 30
    out = ls.scores()
    assert chamadas["n"] == 1
    assert out == {("BRA", "HAI"): (3, 0)}
