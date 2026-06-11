from app.copa import CopaService, JANELA_S
from app.sources.openfootball import Match

T0 = 1_781_000_000  # "agora" de referência dos testes


def jogo(epoch, t1="AAA", t2="BBB", **kw):
    base = dict(epoch=epoch, dia=1, mes=6, h=12, m=0, t1=t1, t2=t2, info="Grupo X")
    base.update(kw)
    return Match(**base)


class FetcherFake:
    def __init__(self, idade=42):
        self.idade = idade

    def get(self):
        return {"matches": []}, self.idade


def monta(matches, agora=T0):
    svc = CopaService(FetcherFake(), now_fn=lambda: agora)
    svc._parse = lambda data: matches  # parser substituído por dados sintéticos
    return svc


def test_proximos_ordena_e_corta_em_n():
    ms = [jogo(T0 + 3600), jogo(T0 + 60), jogo(T0 + 7200)]
    out, idade = monta(ms).proximos(2)
    assert [m.epoch for m in out] == [T0 + 60, T0 + 3600]
    assert idade == 42


def test_jogo_rolando_ainda_conta_como_proximo():
    # começou há 1h: ainda dentro da janela de 130 min
    out, _ = monta([jogo(T0 - 3600)]).proximos(5)
    assert len(out) == 1


def test_jogo_encerrado_sai_da_lista():
    out, _ = monta([jogo(T0 - JANELA_S - 1)]).proximos(5)
    assert out == []


def test_n_clampado_em_20():
    ms = [jogo(T0 + 100 + i) for i in range(30)]
    out, _ = monta(ms).proximos(99)
    assert len(out) == 20


def test_brasil_filtra_dos_dois_lados():
    ms = [jogo(T0 + 100, "BRA", "MAR"), jogo(T0 + 200, "MEX", "RSA"),
          jogo(T0 + 300, "SCO", "BRA")]
    out, _ = monta(ms).brasil()
    assert [(m.t1, m.t2) for m in out] == [("BRA", "MAR"), ("SCO", "BRA")]


def test_live_so_dentro_da_janela():
    ms = [jogo(T0 - 60),               # rolando
          jogo(T0 + 60),               # ainda não começou
          jogo(T0 - JANELA_S - 1)]     # já acabou
    out, _ = monta(ms).live()
    assert len(out) == 1 and out[0].epoch == T0 - 60
