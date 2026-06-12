import json

from app.memoria import MemoriaService, MAX_PARES, TETO, VARRE


def svc(tmp_path, chat_fn=None, now=1000):
    return MemoriaService(base_dir=tmp_path, chat_fn=chat_fn,
                          now_fn=lambda: now)


def test_grava_par_cria_arquivo_em_ordem(tmp_path):
    m = svc(tmp_path)
    m.grava_par("k1", "oi", "olá!")
    m.grava_par("k1", "tudo bem?", "to ótimo")
    dirs = list((tmp_path / "claw").iterdir())
    assert len(dirs) == 1
    reg = json.loads((dirs[0] / "registro.json").read_text(encoding="utf-8"))
    assert [p["q"] for p in reg["pares"]] == ["oi", "tudo bem?"]
    assert reg["pares"][0]["ts"] == 1000
    assert reg["resumidos"] == 0


def test_id8_estavel_e_sem_a_chave(tmp_path):
    m = svc(tmp_path)
    m.grava_par("chave-secreta", "a", "b")
    m.grava_par("chave-secreta", "c", "d")
    dirs = list((tmp_path / "claw").iterdir())
    assert len(dirs) == 1
    assert "chave-secreta" not in dirs[0].name
    assert len(dirs[0].name) == 8


def test_devices_diferentes_nao_se_misturam(tmp_path):
    m = svc(tmp_path)
    m.grava_par("k1", "a", "b")
    m.grava_par("k2", "c", "d")
    assert len(list((tmp_path / "claw").iterdir())) == 2


def test_teto_descarta_o_mais_antigo(tmp_path):
    m = svc(tmp_path)
    for i in range(TETO + 1):
        m.grava_par("k1", f"q{i}", f"r{i}")
    reg = m._carrega("k1")
    assert len(reg["pares"]) == TETO
    assert reg["pares"][0]["q"] == "q1"  # q0 caiu


def test_registro_corrompido_vira_vazio(tmp_path):
    m = svc(tmp_path)
    m.grava_par("k1", "a", "b")
    d = next((tmp_path / "claw").iterdir())
    (d / "registro.json").write_text("{lixo", encoding="utf-8")
    assert m._carrega("k1") == {"pares": [], "resumidos": 0}
