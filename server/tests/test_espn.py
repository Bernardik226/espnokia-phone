from datetime import date

import httpx

from app.sources.espn import EspnScores, _autor, _minuto

BASE = "http://espn.test"
HOJE = date(2026, 6, 12)


class RelogioFake:
    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


def time_(abbr, tid, score, lado):
    return {"homeAway": lado, "score": str(score),
            "team": {"id": tid, "abbreviation": abbr}}


def gol(tid, nome, minuto="59'", shootout=False):
    return {"scoringPlay": True, "shootout": shootout,
            "clock": {"displayValue": minuto},
            "team": {"id": tid},
            "athletesInvolved": [{"shortName": nome}]}


def evento(casa, fora, s1=0, s2=0, estado="in", clock="67'",
           details=None, venue=None):
    comp = {"competitors": [time_(casa, "1", s1, "home"),
                            time_(fora, "2", s2, "away")],
            "status": {"type": {"state": estado}, "displayClock": clock},
            "details": details or []}
    if venue:
        comp["venue"] = venue
    return {"competitions": [comp]}


def monta(rotas, clock=None):
    """Roteia por trecho do path ("/scoreboard", "/standings")."""
    chamadas = {"n": 0, "urls": []}

    def handler(request):
        chamadas["n"] += 1
        chamadas["urls"].append(str(request.url))
        for trecho, corpo in rotas.items():
            if trecho in request.url.path:
                return httpx.Response(200, json=corpo)
        return httpx.Response(404)

    es = EspnScores("fifa.world", BASE, transport=httpx.MockTransport(handler),
                    clock=clock or RelogioFake(), hoje=lambda: HOJE)
    return es, chamadas


def test_janela_de_datas_ontem_a_amanha():
    # O "dia" da ESPN é fuso dos EUA: jogo de madrugada cai no dia vizinho.
    es, ch = monta({"/scoreboard": {"events": []}})
    es.jogos()
    assert "dates=20260611-20260613" in ch["urls"][0]
    assert "/soccer/fifa.world/scoreboard" in ch["urls"][0]


def test_jogo_ao_vivo_traz_placar_minuto_gols_e_estadio():
    ev = evento("MEX", "RSA", 2, 0,
                details=[gol("1", "J. Quinones", "9'"),
                         gol("1", "R. Jimenez", "67'")],
                venue={"fullName": "Estadio Azteca",
                       "address": {"city": "Mexico City"}})
    es, _ = monta({"/scoreboard": {"events": [ev]}})
    j = es.jogos()[("MEX", "RSA")]
    assert (j["s1"], j["s2"], j["min"]) == (2, 0, "67")
    assert j["rolando"] is True and j["fim"] is False
    assert j["g1"] == ["J. Quinones 9'", "R. Jimenez 67'"]
    assert j["g2"] == []
    assert j["est"] == "Estadio Azteca · Mexico City"


def test_par_entra_nas_duas_ordens():
    # A tabela local pode inverter mandante: o espelho troca placar e gols.
    ev = evento("MEX", "RSA", 2, 1, details=[gol("2", "Fulano", "12'")])
    es, _ = monta({"/scoreboard": {"events": [ev]}})
    espelho = es.jogos()[("RSA", "MEX")]
    assert (espelho["s1"], espelho["s2"]) == (1, 2)
    assert espelho["g1"] == ["Fulano 12'"]
    assert espelho["g2"] == []


def test_estados_pre_e_post():
    es, _ = monta({"/scoreboard": {"events": [
        evento("BRA", "MAR", estado="pre", clock="0'"),
        evento("FRA", "SEN", 1, 1, estado="post", clock="90'"),
    ]}})
    antes = es.jogos()[("BRA", "MAR")]
    assert antes["rolando"] is False and antes["fim"] is False
    depois = es.jogos()[("FRA", "SEN")]
    assert depois["fim"] is True
    assert depois["min"] == ""  # minuto só faz sentido com bola rolando


def test_minuto_tira_os_apostrofos():
    assert _minuto({"displayClock": "72'"}) == "72"
    assert _minuto({"displayClock": "45'+3'"}) == "45+3"
    assert _minuto({}) == ""


def test_autor_cai_pro_nome_completo():
    assert _autor({"athletesInvolved": [{"displayName": "Fulano de Tal"}],
                   "clock": {"displayValue": "12'"}}) == "Fulano de Tal 12'"
    assert _autor({}) == "?"


def test_penaltis_da_decisao_nao_contam_como_gol():
    ev = evento("ARG", "FRA", 3, 3,
                details=[gol("1", "Messi", "23'"),
                         gol("1", "Fulano", shootout=True)])
    es, _ = monta({"/scoreboard": {"events": [ev]}})
    assert es.jogos()[("ARG", "FRA")]["g1"] == ["Messi 23'"]


def test_time_sem_codigo_e_ignorado():
    quebrado = evento("X", "Y")
    quebrado["competitions"][0]["competitors"][0]["team"].pop("abbreviation")
    es, _ = monta({"/scoreboard": {"events": [quebrado,
                                              evento("MEX", "RSA", 1, 0)]}})
    assert set(es.jogos()) == {("MEX", "RSA"), ("RSA", "MEX")}


def test_cache_de_60s_evita_rede():
    clock = RelogioFake()
    es, ch = monta({"/scoreboard": {"events": [evento("BRA", "HAI", 3, 0)]}},
                   clock=clock)
    es.jogos()
    clock.t += 30
    assert es.jogos()[("BRA", "HAI")]["s1"] == 3
    assert ch["n"] == 1


def test_erro_na_fonte_mantem_o_cache_anterior():
    clock = RelogioFake()
    rotas = {"/scoreboard": {"events": [evento("BRA", "HAI", 1, 0)]}}
    es, _ = monta(rotas, clock=clock)
    assert es.jogos()[("BRA", "HAI")]["s1"] == 1
    rotas.pop("/scoreboard")                 # fonte caiu (vira 404)
    clock.t += 120                           # cache de 60 s já venceu
    assert es.jogos()[("BRA", "HAI")]["s1"] == 1  # placar velho > nenhum


def entrada(abbr, pts, j, sg, rank):
    return {"team": {"abbreviation": abbr},
            "stats": [{"name": "points", "value": pts},
                      {"name": "gamesPlayed", "value": j},
                      {"name": "pointDifferential", "value": sg},
                      {"name": "rank", "value": rank}]}


def test_grupos_seguem_o_rank_da_fonte():
    es, _ = monta({"/standings": {"children": [
        {"name": "Group B", "standings": {"entries": [
            entrada("KOR", 3, 1, 1, 1)]}},
        {"name": "Group A", "standings": {"entries": [
            entrada("MEX", 3, 1, 0, 2),
            entrada("RSA", 3, 1, 2, 1)]}},
    ]}})
    gs = es.grupos()
    assert [g["n"] for g in gs] == ["A", "B"]  # "Group A" encurta pra "A"
    assert [t["c"] for t in gs[0]["t"]] == ["RSA", "MEX"]  # ordem do rank
    assert gs[0]["t"][0] == {"c": "RSA", "pts": 3, "j": 1, "sg": 2}


def test_grupos_cache_de_300s_e_erro_mantem_cache():
    clock = RelogioFake()
    rotas = {"/standings": {"children": [
        {"name": "Group A", "standings": {"entries": []}}]}}
    es, ch = monta(rotas, clock=clock)
    assert es.grupos()[0]["n"] == "A"
    clock.t += 200
    es.grupos()
    assert ch["n"] == 1                      # dentro do TTL: sem rede
    rotas.pop("/standings")                  # fonte caiu
    clock.t += 200                           # agora o TTL de 300 s venceu
    assert es.grupos()[0]["n"] == "A"        # cache segura a classificação
