import json

from fastapi.testclient import TestClient

from app.claude_voz import VozService
from app.main import create_app
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


CFG_FAKE = {"persona": "Você é o Claude, um bichinho.",
            "anthropic_api_key": "x", "claude_model": "m"}


def enche(m, device, n):
    for i in range(n):
        m.grava_par(device, f"pergunta {i}", f"resposta {i}")


def test_precisa_resumo_no_limiar(tmp_path):
    m = svc(tmp_path)
    enche(m, "k1", MAX_PARES - 1)
    assert not m.precisa_resumo("k1")
    m.grava_par("k1", "a", "b")
    assert m.precisa_resumo("k1")


def test_resumir_funde_e_varre(tmp_path):
    prompts = []

    def chat_spy(cfg, system, mensagens):
        prompts.append((system, mensagens[0]["content"]))
        return "memoria nova"

    m = svc(tmp_path, chat_fn=chat_spy)
    enche(m, "k1", MAX_PARES)
    m.resumir("k1", "pt", CFG_FAKE)
    system, user = prompts[0]
    assert CFG_FAKE["persona"] in system
    assert "(ainda vazia)" in user          # primeira memória
    assert "pergunta 0" in user             # os antigos entram
    assert f"pergunta {VARRE - 1}" in user
    assert f"pergunta {VARRE}" not in user  # os recentes não
    reg = m._carrega("k1")
    assert len(reg["pares"]) == MAX_PARES - VARRE
    assert reg["pares"][0]["q"] == f"pergunta {VARRE}"
    assert reg["resumidos"] == VARRE
    assert m.memoria_texto("k1") == "memoria nova"


def test_resumir_acumula_memoria_anterior(tmp_path):
    user_prompts = []

    def chat_spy(cfg, system, mensagens):
        user_prompts.append(mensagens[0]["content"])
        return "memoria v2"

    m = svc(tmp_path, chat_fn=chat_spy)
    enche(m, "k1", MAX_PARES)
    m.resumir("k1", "pt", CFG_FAKE)
    enche(m, "k1", VARRE)               # enche de novo até 30
    m.resumir("k1", "pt", CFG_FAKE)
    assert "memoria v2" in user_prompts[1]
    reg = m._carrega("k1")
    assert reg["resumidos"] == 2 * VARRE


def test_resumir_nao_perde_par_gravado_durante_a_chamada(tmp_path):
    m = svc(tmp_path)

    def chat_durante(cfg, system, mensagens):
        # simula push-to-talk chegando enquanto a API pensa
        m.grava_par("k1", "cheguei no meio", "opa!")
        return "memoria nova"

    m.chat_fn = chat_durante
    enche(m, "k1", MAX_PARES)
    m.resumir("k1", "pt", CFG_FAKE)
    reg = m._carrega("k1")
    qs = [p["q"] for p in reg["pares"]]
    assert "cheguei no meio" in qs          # o par novo sobreviveu
    assert len(reg["pares"]) == MAX_PARES - VARRE + 1


def test_resumir_corta_no_limite(tmp_path):
    m = svc(tmp_path, chat_fn=lambda *a: "x" * 4000)
    enche(m, "k1", MAX_PARES)
    m.resumir("k1", "pt", CFG_FAKE)
    assert len(m.memoria_texto("k1")) == 1500


def test_pares_pagina_mais_recente_primeiro(tmp_path):
    m = svc(tmp_path)
    enche(m, "k1", 14)                  # 14 pares -> 3 páginas (6+6+2)
    p0 = m.pares("k1", 0)
    assert p0["total"] == 14 and p0["pags"] == 3 and p0["pag"] == 0
    assert p0["itens"][0]["q"] == "pergunta 13"   # mais recente primeiro
    assert len(p0["itens"]) == 6
    p2 = m.pares("k1", 2)
    assert len(p2["itens"]) == 2
    assert p2["itens"][-1]["q"] == "pergunta 0"   # o mais antigo no fim


def test_pares_tem_data_e_hora(tmp_path):
    m = svc(tmp_path)
    m.grava_par("k1", "oi", "olá")
    it = m.pares("k1", 0)["itens"][0]
    assert len(it["d"]) == 5 and it["d"][2] == "/"   # dd/mm
    assert len(it["h"]) == 5 and it["h"][2] == ":"   # hh:mm


def test_pares_vazio(tmp_path):
    assert svc(tmp_path).pares("k1", 0) == {
        "total": 0, "pags": 0, "pag": 0, "itens": []}


def test_pares_trunca_em_bytes_sem_quebrar_utf8(tmp_path):
    m = svc(tmp_path)
    m.grava_par("k1", "é" * 200, "ã" * 300)   # 2 bytes por char
    it = m.pares("k1", 0)["itens"][0]
    assert it["q"].endswith("…")
    assert len(it["q"].encode("utf-8")) <= 160
    assert len(it["r"].encode("utf-8")) <= 280
    it["q"].encode("utf-8").decode("utf-8")   # não quebrou multibyte


def test_memoria_payload(tmp_path):
    m = svc(tmp_path, chat_fn=lambda *a: "lembro de tudo")
    assert m.memoria("k1") == {"memoria": "", "resumidos": 0, "ts": 0}
    enche(m, "k1", MAX_PARES)
    m.resumir("k1", "pt", CFG_FAKE)
    pay = m.memoria("k1")
    assert pay["memoria"] == "lembro de tudo"
    assert pay["resumidos"] == VARRE
    assert pay["ts"] > 0


def test_recentes_em_formato_de_mensagens(tmp_path):
    m = svc(tmp_path)
    enche(m, "k1", 8)
    msgs = m.recentes("k1", n=6)
    assert len(msgs) == 12
    assert msgs[0] == {"role": "user", "content": "pergunta 2"}
    assert msgs[-1] == {"role": "assistant", "content": "resposta 7"}


def test_registro_com_shape_errado_vira_vazio(tmp_path):
    m = svc(tmp_path)
    m.grava_par("k1", "a", "b")
    d = next((tmp_path / "claw").iterdir())
    (d / "registro.json").write_text('{"pares": null}', encoding="utf-8")
    assert m._carrega("k1") == {"pares": [], "resumidos": 0}
    (d / "registro.json").write_text("[]", encoding="utf-8")
    assert m._carrega("k1") == {"pares": [], "resumidos": 0}


def faz_client_mem(tmp_path, monkeypatch, chat_fn=lambda *a: ("oi!", 1, 1)):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.delenv("ANTHROPIC_API_KEY", raising=False)
    mem = MemoriaService(base_dir=tmp_path,
                         chat_fn=lambda *a: "resumo do claw'd")
    voz = VozService(stt_fn=lambda *a: "oi", chat_fn=chat_fn, memoria=mem)
    return TestClient(create_app(device_keys="k1", voz=voz)), mem


def test_rota_registro_paginada(tmp_path, monkeypatch):
    client, mem = faz_client_mem(tmp_path, monkeypatch)
    enche(mem, "k1", 7)
    r = client.get("/claude/registro?pag=1",
                   headers={"X-Device-Key": "k1"})
    assert r.status_code == 200
    body = r.json()
    assert body["total"] == 7 and body["pags"] == 2 and body["pag"] == 1
    assert len(body["itens"]) == 1


def test_rota_registro_sem_chave_401(tmp_path, monkeypatch):
    client, _ = faz_client_mem(tmp_path, monkeypatch)
    assert client.get("/claude/registro").status_code == 401


def test_rota_memoria(tmp_path, monkeypatch):
    client, mem = faz_client_mem(tmp_path, monkeypatch)
    r = client.get("/claude/memoria", headers={"X-Device-Key": "k1"})
    assert r.status_code == 200
    assert r.json() == {"memoria": "", "resumidos": 0, "ts": 0}


def test_voz_dispara_resumo_na_margem(tmp_path, monkeypatch):
    client, mem = faz_client_mem(tmp_path, monkeypatch)
    enche(mem, "k1", MAX_PARES - 1)     # a próxima conversa fecha 30
    r = client.post("/claude/voz?lang=pt", content=b"\x00" * 64,
                    headers={"X-Device-Key": "k1"})
    assert r.status_code == 200
    # TestClient roda os BackgroundTasks antes de devolver:
    assert mem.memoria_texto("k1") == "resumo do claw'd"
    assert len(mem._carrega("k1")["pares"]) == MAX_PARES - VARRE


def test_resumo_falhando_nao_quebra_a_voz(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.delenv("ANTHROPIC_API_KEY", raising=False)

    def chat_explode(*a):
        raise RuntimeError("api fora")

    mem = MemoriaService(base_dir=tmp_path, chat_fn=chat_explode)
    voz = VozService(stt_fn=lambda *a: "oi",
                     chat_fn=lambda *a: ("oi!", 1, 1), memoria=mem)
    client = TestClient(create_app(device_keys="k1", voz=voz))
    enche(mem, "k1", MAX_PARES - 1)
    r = client.post("/claude/voz?lang=pt", content=b"\x00" * 64,
                    headers={"X-Device-Key": "k1"})
    assert r.status_code == 200          # a resposta sai mesmo assim
    assert mem.memoria_texto("k1") == ""  # resumo não rolou, sem drama
