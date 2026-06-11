import json
import pathlib

from app.sources.openfootball import parse_matches

FIX = pathlib.Path(__file__).parent / "fixtures" / "worldcup_sample.json"


def carrega():
    return parse_matches(json.loads(FIX.read_text()))


def test_converte_horario_para_brasilia():
    # "2026-06-13" "18:00 UTC-4" (Brazil x Morocco) → 19:00 em Brasília
    m = [x for x in carrega() if x.t1 == "BRA"][0]
    assert (m.dia, m.mes, m.h, m.m) == (13, 6, 19, 0)


def test_fuso_do_mexico_tambem_converte():
    # abertura: "13:00 UTC-6" → 16:00 BRT
    m = [x for x in carrega() if x.t1 == "MEX"][0]
    assert (m.dia, m.mes, m.h, m.m) == (11, 6, 16, 0)


def test_mapeia_nomes_para_codigos_fifa():
    nomes = {(x.t1, x.t2) for x in carrega()}
    assert ("BRA", "MAR") in nomes
    assert ("MEX", "RSA") in nomes


def test_placeholder_do_mata_mata_passa_direto():
    ko = [x for x in carrega() if x.info.startswith("Round")]
    assert ko and ko[0].t1 == "2A"


def test_grupo_vira_rotulo_em_portugues():
    m = [x for x in carrega() if x.t1 == "BRA"][0]
    assert m.info == "Grupo C"


def test_sem_placar_vira_menos_um():
    m = carrega()[0]
    assert m.s1 == -1 and m.s2 == -1


def test_ordenado_por_kickoff_com_epoch_utc():
    ms = carrega()
    assert [x.epoch for x in ms] == sorted(x.epoch for x in ms)
    bra = [x for x in ms if x.t1 == "BRA"][0]
    assert bra.epoch == 1781388000  # 2026-06-13 22:00:00 UTC


def test_todas_as_48_selecoes_tem_codigo():
    from app.teams import NAME_TO_CODE
    assert len(NAME_TO_CODE) == 48
    assert all(len(c) == 3 for c in NAME_TO_CODE.values())
