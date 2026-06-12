from app.sources.cadeia import CadeiaDeFontes

JOGO = {("BRA", "HAI"): {"s1": 1, "s2": 0}}
RESERVA = {("BRA", "HAI"): {"s1": 9, "s2": 9}}
GRUPOS = [{"n": "A", "t": []}]


class FonteFake:
    def __init__(self, jogos=None, grupos=None, explode=False):
        self._j, self._g, self.explode = jogos or {}, grupos or [], explode

    def jogos(self):
        if self.explode:
            raise RuntimeError("fora do ar")
        return self._j

    def grupos(self):
        if self.explode:
            raise RuntimeError("fora do ar")
        return self._g


def test_primeira_fonte_com_conteudo_ganha():
    c = CadeiaDeFontes([FonteFake(jogos=JOGO, grupos=GRUPOS),
                        FonteFake(jogos=RESERVA)])
    assert c.jogos() == JOGO
    assert c.grupos() == GRUPOS


def test_fonte_vazia_passa_a_vez():
    c = CadeiaDeFontes([FonteFake(), FonteFake(jogos=RESERVA, grupos=GRUPOS)])
    assert c.jogos() == RESERVA
    assert c.grupos() == GRUPOS


def test_fonte_quebrada_passa_a_vez():
    c = CadeiaDeFontes([FonteFake(explode=True),
                        FonteFake(jogos=RESERVA, grupos=GRUPOS)])
    assert c.jogos() == RESERVA
    assert c.grupos() == GRUPOS


def test_todas_mortas_devolvem_o_ultimo_bom():
    boa = FonteFake(jogos=JOGO, grupos=GRUPOS)
    c = CadeiaDeFontes([boa])
    c.jogos()
    c.grupos()
    boa.explode = True                       # tudo caiu depois do 1º fetch
    assert c.jogos() == JOGO
    assert c.grupos() == GRUPOS


def test_sem_nada_em_lugar_nenhum_volta_vazio():
    c = CadeiaDeFontes([FonteFake(explode=True), FonteFake()])
    assert c.jogos() == {}
    assert c.grupos() == []
