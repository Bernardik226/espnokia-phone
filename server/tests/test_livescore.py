import httpx

from app.sources.livescore import LiveScores, _parse_scorers

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


def monta_rotas(rotas, clock=None):
    """Como monta(), mas roteando por path — a classe fala com 3 endpoints."""
    chamadas = {"n": 0}

    def handler(request):
        chamadas["n"] += 1
        corpo = rotas.get(request.url.path)
        if corpo is None:
            return httpx.Response(404)
        return httpx.Response(200, json=corpo)

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


def test_parse_scorers():
    assert _parse_scorers("null") == []
    assert _parse_scorers(None) == []
    # Formato real da fonte: aspas tipográficas misturadas
    assert _parse_scorers("{“J. Quiñones 9'”,”R. Jiménez 67'”}") == \
        ["J. Quiñones 9'", "R. Jiménez 67'"]
    assert _parse_scorers('{"A. Silva 12\'"}') == ["A. Silva 12'"]


def test_jogos_traz_minuto_gols_e_estadio():
    g = jogo("Mexico", "South Africa", 2, 0, elapsed="67")
    g["home_scorers"] = "{“J. Quiñones 9'”,”R. Jiménez 67'”}"
    g["away_scorers"] = "null"
    g["stadium_id"] = "1"
    ls, _ = monta_rotas({
        "/get/games": {"games": [g]},
        "/get/stadiums": {"stadiums": [
            {"id": "1", "name_en": "Estadio Azteca", "city_en": "Mexico City"}]},
    })
    j = ls.jogos()[("MEX", "RSA")]
    assert (j["s1"], j["s2"], j["min"]) == (2, 0, "67")
    assert j["rolando"] is True and j["fim"] is False
    assert j["g1"] == ["J. Quiñones 9'", "R. Jiménez 67'"]
    assert j["g2"] == []
    assert j["est"] == "Estadio Azteca · Mexico City"


def test_jogos_erro_mantem_cache_anterior():
    clock = RelogioFake()
    rotas = {"/get/games": {"games": [jogo("Brazil", "Haiti", 1, 0)]}}
    ls, _ = monta_rotas(rotas, clock=clock)
    assert ls.jogos()[("BRA", "HAI")]["s1"] == 1
    rotas.pop("/get/games")                  # fonte caiu
    clock.t += 120                           # cache de 60 s já venceu
    assert ls.jogos()[("BRA", "HAI")]["s1"] == 1  # placar velho > placar nenhum


def test_mata_mata_sem_times_definidos_e_ignorado():
    # O feed real traz os 32 jogos do mata-mata sem nomes ("Runner-up Group A")
    sem_nome = {"home_team_id": "0", "away_team_id": "0",
                "home_score": "0", "away_score": "0",
                "finished": "FALSE", "time_elapsed": "notstarted"}
    ls, _ = monta(payload={"games": [sem_nome,
                                     jogo("Brazil", "Haiti", 1, 0)]})
    assert list(ls.jogos()) == [("BRA", "HAI")]


def test_estadio_fora_do_ar_nao_derruba_jogos():
    g = jogo("Brazil", "Haiti", 1, 0)
    g["stadium_id"] = "7"
    ls, _ = monta_rotas({"/get/games": {"games": [g]}})  # sem /get/stadiums
    assert ls.jogos()[("BRA", "HAI")]["est"] == ""


def time_grupo(tid, pts, sg, mp=1):
    return {"team_id": tid, "pts": str(pts), "gd": str(sg), "mp": str(mp)}


def test_grupos_classifica_por_pontos_e_saldo():
    ls, _ = monta_rotas({
        "/get/groups": {"groups": [
            {"name": "B", "teams": [time_grupo("3", 3, 1)]},
            {"name": "A", "teams": [time_grupo("1", 3, 0),
                                    time_grupo("2", 3, 2)]},
        ]},
        "/get/teams": {"teams": [{"id": "1", "fifa_code": "MEX"},
                                 {"id": "2", "fifa_code": "RSA"},
                                 {"id": "3", "fifa_code": "KOR"}]},
    })
    gs = ls.grupos()
    assert [g["n"] for g in gs] == ["A", "B"]
    # mesmo nº de pontos → desempata no saldo
    assert [t["c"] for t in gs[0]["t"]] == ["RSA", "MEX"]
    assert gs[0]["t"][0] == {"c": "RSA", "pts": 3, "j": 1, "sg": 2}


def test_grupos_cache_de_300s():
    clock = RelogioFake()
    ls, chamadas = monta_rotas({
        "/get/groups": {"groups": []},
        "/get/teams": {"teams": []},
    }, clock=clock)
    ls.grupos()
    clock.t += 200
    ls.grupos()
    assert chamadas["n"] == 1  # groups (teams nem foi buscado: 0 grupos)
