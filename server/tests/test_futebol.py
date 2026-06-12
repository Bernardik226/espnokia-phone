from datetime import datetime, timezone

from app.futebol import FutebolService, payload

AGORA = datetime(2026, 6, 12, 20, 30, tzinfo=timezone.utc)


def partida(t1, t2, hora=20, dia=12, rolando=False, fim=False, **extra):
    p = {"data": datetime(2026, 6, dia, hora, 0, tzinfo=timezone.utc),
         "t1": t1, "t2": t2, "n1": "", "n2": "", "s1": 0, "s2": 0,
         "rolando": rolando, "fim": fim, "min": "", "g1": [], "g2": [],
         "est": ""}
    p.update(extra)
    return p


class FonteFake:
    def __init__(self, ps=None, explode=False):
        self.ps, self.explode = ps or [], explode

    def partidas(self):
        if self.explode:
            raise RuntimeError("fora do ar")
        return self.ps


def monta(fontes):
    return FutebolService(fontes, now_fn=lambda: AGORA)


def test_ligas_so_as_que_tem_jogo_na_ordem_do_catalogo():
    svc = monta({"eng.1": FonteFake([partida("ARS", "LIV")]),
                 "bra.1": FonteFake([partida("FLA", "VAS")]),
                 "esp.1": FonteFake()})                    # sem jogos: some
    assert svc.ligas() == [{"id": "bra.1", "n": "Brasileirão"},
                           {"id": "eng.1", "n": "Premier League"}]


def test_liga_quebrada_nao_derruba_o_cardapio():
    svc = monta({"bra.1": FonteFake(explode=True),
                 "eng.1": FonteFake([partida("ARS", "LIV")])})
    assert svc.ligas() == [{"id": "eng.1", "n": "Premier League"}]


def test_jogos_corta_pra_8_sem_derrubar_quem_esta_rolando():
    ps = [partida("RO%d" % i, "X%d" % i, hora=10, dia=11) for i in range(2)]
    ps[0]["rolando"] = True                # antigo MAS rolando: não cai
    ps += [partida("FU%d" % i, "Y%d" % i, hora=18, dia=13)
           for i in range(9)]              # 9 futuros distantes
    svc = monta({"bra.1": FonteFake(ps)})
    out = svc.jogos("bra.1")
    assert len(out) == 8
    assert ("RO0", "X0") in [(p["t1"], p["t2"]) for p in out]
    assert out == sorted(out, key=lambda p: p["data"])  # devolve cronológico


def test_liga_desconhecida_volta_vazio():
    assert monta({}).jogos("xyz.9") == []


def test_live_pega_rolando_e_fim_recente():
    # entrada cronológica, como partidas() garante
    svc = monta({"bra.1": FonteFake([
        partida("GRE", "INT", hora=10, dia=11, fim=True),  # ontem: fora
        partida("PAL", "COR", hora=18, fim=True),     # acabou há pouco
        partida("FLA", "VAS", hora=20, rolando=True),
        partida("SAO", "SAN", hora=23),                    # nem começou
    ])})
    assert [(p["t1"], p["t2"]) for p in svc.live("bra.1")] == \
        [("PAL", "COR"), ("FLA", "VAS")]


def test_payload_no_contrato_da_copa():
    ps = [partida("FLA", "VAS", hora=20, rolando=True, s1=2, s2=1,
                  n1="Flamengo", n2="Vasco", min="67",
                  g1=["Pedro 12'", "B. Henrique 50'"],
                  est="Maracanã · Rio de Janeiro"),
          partida("SAO", "SAN", hora=23)]
    body = payload(ps, "Brasileirão")
    j = body["jogos"][0]
    # 20:00 UTC → 17:00 em Brasília
    assert (j["dia"], j["mes"], j["h"], j["m"]) == (12, 6, 17, 0)
    assert (j["t1"], j["n1"], j["n2"]) == ("FLA", "Flamengo", "Vasco")
    assert (j["s1"], j["s2"], j["live"], j["min"]) == (2, 1, True, "67")
    assert j["g1"] == "Pedro 12'\nB. Henrique 50'"
    assert "g2" not in j
    assert j["info"] == "Brasileirão"
    assert j["est"] == "Maracanã · Rio de Janeiro"
    frio = body["jogos"][1]
    assert (frio["s1"], frio["s2"], frio["live"]) == (-1, -1, False)
    assert "min" not in frio and "n1" not in frio
