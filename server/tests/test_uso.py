import json
import time

from app import config, uso


def escreve(tmp_path, linhas):
    (tmp_path / "uso.jsonl").write_text(
        "".join(json.dumps(x) + "\n" for x in linhas), encoding="utf-8")


def linha(device, ts, usd, ti=10, to=20, buscas=0):
    return {"ts": ts, "device": config.id8(device),
            "tokens_in": ti, "tokens_out": to, "buscas": buscas,
            "custo_usd": usd}


JUN = int(time.mktime((2026, 6, 15, 12, 0, 0, 0, 0, -1)))   # dia no mês passado
JUL_A = int(time.mktime((2026, 7, 10, 12, 0, 0, 0, 0, -1)))
JUL_B = int(time.mktime((2026, 7, 11, 12, 0, 0, 0, 0, -1)))
AGORA = int(time.mktime((2026, 7, 16, 12, 0, 0, 0, 0, -1)))


def test_vazio_zera(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    u = uso.agrega("k1", now_fn=lambda: AGORA)
    assert u["total_usd"] == 0 and u["falas"] == 0 and u["serie"] == []
    assert u["medio_usd"] == 0 and u["mes_pct"] is None


def test_totais_serie_e_mes(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    escreve(tmp_path, [
        linha("k1", JUN, 0.01),          # mês passado
        linha("k1", JUL_A, 0.02),
        linha("k1", JUL_A, 0.03),        # mesmo dia soma
        linha("k1", JUL_B, 0.04),
        linha("outra", JUL_B, 9.99),     # outro device: ignorado
    ])
    u = uso.agrega("k1", now_fn=lambda: AGORA)
    assert u["falas"] == 4
    assert u["total_usd"] == 0.1                 # 0.01+0.02+0.03+0.04
    assert u["mes_usd"] == 0.09                  # só julho
    assert u["tokens_in"] == 40 and u["tokens_out"] == 80
    assert u["medio_usd"] == round(0.1/4, 6)
    dias = {d["d"]: d for d in u["serie"]}
    assert dias["10/07"]["usd"] == 0.05 and dias["10/07"]["falas"] == 2
    assert dias["11/07"]["usd"] == 0.04
    assert "15/06" in dias                       # entra na série (dentro de 30 dias-registro)


def test_orcamento_e_pct(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    config.save({"orcamento_usd_mes": 0.10})
    escreve(tmp_path, [linha("k1", JUL_A, 0.08)])
    u = uso.agrega("k1", now_fn=lambda: AGORA)
    assert u["orcamento_usd_mes"] == 0.10
    assert u["mes_pct"] == 0.8


def test_estourado_passa_de_1(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    config.save({"orcamento_usd_mes": 0.05})
    escreve(tmp_path, [linha("k1", JUL_A, 0.075)])
    u = uso.agrega("k1", now_fn=lambda: AGORA)
    assert u["mes_pct"] == 1.5


def test_linha_corrompida_ignorada(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    (tmp_path / "uso.jsonl").write_text(
        json.dumps(linha("k1", JUL_A, 0.02)) + "\n{lixo meia linha",
        encoding="utf-8")
    u = uso.agrega("k1", now_fn=lambda: AGORA)
    assert u["falas"] == 1 and u["total_usd"] == 0.02


def test_linha_shape_torto_ignorada(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    (tmp_path / "uso.jsonl").write_text(
        "null\n"                                              # JSON válido, não-dict
        + json.dumps({"device": config.id8("k1"), "ts": "abc",
                      "custo_usd": "x"}) + "\n"               # campos não-numéricos
        + json.dumps(linha("k1", JUL_A, 0.02)) + "\n",       # linha boa
        encoding="utf-8")
    u = uso.agrega("k1", now_fn=lambda: AGORA)
    assert u["falas"] == 1 and u["total_usd"] == 0.02


def test_serie_nao_funde_anos(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    jul_2025 = int(time.mktime((2025, 7, 10, 12, 0, 0, 0, 0, -1)))
    escreve(tmp_path, [linha("k1", jul_2025, 0.01), linha("k1", JUL_A, 0.02)])
    u = uso.agrega("k1", now_fn=lambda: AGORA)
    # mesmo dia/mês, anos diferentes → 2 pontos distintos, não um só somado
    assert len(u["serie"]) == 2
    assert round(sum(p["usd"] for p in u["serie"]), 6) == 0.03
