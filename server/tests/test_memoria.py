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


def test_registro_com_shape_errado_vira_vazio(tmp_path):
    m = svc(tmp_path)
    m.grava_par("k1", "a", "b")
    d = next((tmp_path / "claw").iterdir())
    (d / "registro.json").write_text('{"pares": null}', encoding="utf-8")
    assert m._carrega("k1") == {"pares": [], "resumidos": 0}
    (d / "registro.json").write_text("[]", encoding="utf-8")
    assert m._carrega("k1") == {"pares": [], "resumidos": 0}
