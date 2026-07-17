import json

from fastapi.testclient import TestClient

from app import config, stt
from app.claude_voz import VozService
from app.main import create_app
from app.memoria import MemoriaService


def stt_ok(pcm, lang, cfg):
    return "oi tudo bem"


def chat_ok(cfg, system, mensagens):
    return "miau! to otimo", 42, 17


def faz_client(tmp_path, monkeypatch, stt_fn=stt_ok, chat_fn=chat_ok,
               keys="k1,k2", memoria=None):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.delenv("ANTHROPIC_API_KEY", raising=False)
    voz = VozService(stt_fn=stt_fn, chat_fn=chat_fn,
                     memoria=memoria or MemoriaService(base_dir=tmp_path))
    return TestClient(create_app(device_keys=keys, voz=voz)), voz


def post(client, corpo=b"\x00\x01" * 100, key="k1", lang="pt"):
    return client.post(f"/claude/voz?lang={lang}", content=corpo,
                       headers={"X-Device-Key": key})


def test_sem_chave_de_device_401(tmp_path, monkeypatch):
    client, _ = faz_client(tmp_path, monkeypatch)
    r = client.post("/claude/voz", content=b"x")
    assert r.status_code == 401


def test_caminho_feliz(tmp_path, monkeypatch):
    client, _ = faz_client(tmp_path, monkeypatch)
    r = post(client)
    assert r.status_code == 200
    assert r.json() == {"falei": "oi tudo bem", "resposta": "miau! to otimo"}


def test_corpo_grande_demais_413(tmp_path, monkeypatch):
    client, _ = faz_client(tmp_path, monkeypatch)
    r = post(client, corpo=b"\x00" * (500 * 1024 + 1))
    assert r.status_code == 413


def test_transcricao_vazia_422(tmp_path, monkeypatch):
    client, _ = faz_client(tmp_path, monkeypatch, stt_fn=lambda *a: "")
    assert post(client).status_code == 422


def test_sem_chave_anthropic_502(tmp_path, monkeypatch):
    # chat_fn=None usa o backend real, que exige a chave do config — sem ela,
    # 502 limpo antes de qualquer import do SDK
    client, _ = faz_client(tmp_path, monkeypatch, chat_fn=None)
    assert post(client).status_code == 502


def test_falha_do_claude_502(tmp_path, monkeypatch):
    def explode(cfg, system, mensagens):
        raise RuntimeError("api fora")
    client, _ = faz_client(tmp_path, monkeypatch, chat_fn=explode)
    assert post(client).status_code == 502


def test_falha_do_stt_502(tmp_path, monkeypatch):
    # groq fora do ar / chave errada: 502 limpo em vez de 500 estourado
    def explode(pcm, lang, cfg):
        raise RuntimeError("groq fora")
    client, _ = faz_client(tmp_path, monkeypatch, stt_fn=explode)
    assert post(client).status_code == 502


def test_stt_api_reservado_501(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    cfg = dict(config.DEFAULTS)
    cfg["stt"] = "api"
    (tmp_path / "config.json").write_text(json.dumps(cfg))
    client, _ = faz_client(tmp_path, monkeypatch, stt_fn=stt.transcrever)
    assert post(client).status_code == 501


def test_historico_por_device(tmp_path, monkeypatch):
    visto = []

    def chat_espiao(cfg, system, mensagens):
        visto.append(list(mensagens))
        return "resp", 1, 1

    client, _ = faz_client(tmp_path, monkeypatch, chat_fn=chat_espiao)
    post(client, key="k1")
    post(client, key="k1")
    post(client, key="k2")
    assert len(visto[0]) == 1            # primeira conversa: so o user
    assert len(visto[1]) == 3            # user+assistant da 1a + user novo
    assert visto[1][0]["content"] == "oi tudo bem"
    assert visto[1][1]["role"] == "assistant"
    assert len(visto[2]) == 1            # k2 nao herda o papo do k1


def test_lang_entra_no_system_prompt(tmp_path, monkeypatch):
    visto = {}

    def chat_espiao(cfg, system, mensagens):
        visto["system"] = system
        return "resp", 1, 1

    client, _ = faz_client(tmp_path, monkeypatch, chat_fn=chat_espiao)
    post(client, lang="en")
    assert "idioma en" in visto["system"]
    assert "220" in visto["system"]


def test_hora_do_device_entra_no_system(tmp_path, monkeypatch):
    visto = {}

    def chat_espiao(cfg, system, mensagens):
        visto["system"] = system
        return "resp", 1, 1

    client, _ = faz_client(tmp_path, monkeypatch, chat_fn=chat_espiao)
    client.post("/claude/voz?lang=pt&t=2026-06-12T19:45",
                content=b"\x00\x01" * 100, headers={"X-Device-Key": "k1"})
    assert "2026-06-12T19:45" in visto["system"]
    post(client)  # sem t (RTC sem hora): nada de relogio no system
    assert "relógio" not in visto["system"]


def test_web_search_entra_no_system_por_default(tmp_path, monkeypatch):
    visto = {}

    def chat_espiao(cfg, system, mensagens):
        visto["system"] = system
        return "resp", 1, 1

    client, _ = faz_client(tmp_path, monkeypatch, chat_fn=chat_espiao)
    post(client)
    assert "buscar na internet" in visto["system"]


def test_web_search_desligado_sai_do_system(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    cfg = dict(config.DEFAULTS)
    cfg["web_search"] = False
    (tmp_path / "config.json").write_text(json.dumps(cfg))
    visto = {}

    def chat_espiao(cfg, system, mensagens):
        visto["system"] = system
        return "resp", 1, 1

    client, _ = faz_client(tmp_path, monkeypatch, chat_fn=chat_espiao)
    post(client)
    assert "buscar na internet" not in visto["system"]


def test_uso_jsonl_appendado(tmp_path, monkeypatch):
    client, _ = faz_client(tmp_path, monkeypatch)
    post(client)
    post(client)
    linhas = (tmp_path / "uso.jsonl").read_text().strip().splitlines()
    assert len(linhas) == 2
    uso = json.loads(linhas[0])
    assert uso["device"] == config.id8("k1")
    assert uso["tokens_in"] == 42
    assert uso["tokens_out"] == 17
    assert uso["custo_usd"] > 0
    assert uso["buscas"] == 0


def test_conversa_grava_par_no_registro(tmp_path, monkeypatch):
    client, voz = faz_client(tmp_path, monkeypatch)
    post(client)
    pares = voz.memoria._carrega("k1")["pares"]
    assert pares == [{"ts": pares[0]["ts"], "q": "oi tudo bem",
                      "r": "miau! to otimo"}]


def test_erro_nao_grava_par(tmp_path, monkeypatch):
    client, voz = faz_client(tmp_path, monkeypatch, stt_fn=lambda *a: "")
    post(client)
    assert voz.memoria._carrega("k1")["pares"] == []


def test_memoria_entra_no_system_prompt(tmp_path, monkeypatch):
    systems = []

    def chat_spy(cfg, system, mensagens):
        systems.append(system)
        return "oi!", 1, 1

    mem = MemoriaService(base_dir=tmp_path)
    client, _ = faz_client(tmp_path, monkeypatch, chat_fn=chat_spy,
                           memoria=mem)
    post(client)
    assert "memórias" not in systems[0]        # primeira vida: sem bloco
    (mem._dir("k1") / "memoria.md").write_text("gosto de bolo",
                                               encoding="utf-8")
    post(client)
    assert "gosto de bolo" in systems[1]


def test_historico_sobrevive_a_restart(tmp_path, monkeypatch):
    mensagens_vistas = []

    def chat_spy(cfg, system, mensagens):
        mensagens_vistas.append(list(mensagens))
        return "lembro sim", 1, 1

    client, _ = faz_client(tmp_path, monkeypatch, chat_fn=chat_spy)
    post(client)
    # "restart": client novo, outra VozService, mesmo DATA_DIR
    client2, _ = faz_client(tmp_path, monkeypatch, chat_fn=chat_spy)
    post(client2)
    ultima = mensagens_vistas[-1]
    assert ultima[0] == {"role": "user", "content": "oi tudo bem"}
    assert ultima[1] == {"role": "assistant", "content": "lembro sim"}
    assert ultima[-1] == {"role": "user", "content": "oi tudo bem"}


def test_resposta_longa_bate_na_fala_e_no_registro(tmp_path, monkeypatch):
    # Claude estourou o limite: a fala em tempo real e o registro têm que
    # mostrar EXATAMENTE o mesmo texto cortado (senão o aparelho buga)
    from app.memoria import MAX_R_BYTES
    longa = "miau " * 100                      # ~500 bytes, bem acima do teto
    client, _ = faz_client(tmp_path, monkeypatch,
                           chat_fn=lambda c, s, m: (longa, 1, 1))
    resp = post(client).json()["resposta"]
    assert len(resp.encode("utf-8")) <= MAX_R_BYTES
    reg = client.get("/claude/registro",
                     headers={"X-Device-Key": "k1"}).json()
    assert reg["itens"][0]["r"] == resp        # fala == histórico, byte a byte


def test_e2e_chave_aparelho_dashboard(tmp_path, monkeypatch):
    # fluxo real: o aparelho gera a chave (esp_random), fala usando ela, e o
    # navegador que escaneou o QR (mesma chave) acessa os dados desse device.
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    K = "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6"        # 32 hex, como a do device
    voz = VozService(stt_fn=lambda *a: "oi claw'd",
                     chat_fn=lambda *a: ("miau, oi!", 1, 1),
                     memoria=MemoriaService(base_dir=tmp_path))
    c = TestClient(create_app(device_keys="*", voz=voz))   # capability ligado

    # 1) o aparelho fala -> grava sob hash(K)
    r = c.post("/claude/voz", content=b"\x00\x01" * 50, headers={"X-Device-Key": K})
    assert r.status_code == 200

    # 2) o dashboard, com a MESMA chave, vê status + registro + edita config
    st = c.get("/admin/status", headers={"X-Device-Key": K}).json()
    assert st["conversas"] == 1
    reg = c.get("/claude/registro", headers={"X-Device-Key": K}).json()
    assert (reg["itens"][0]["q"], reg["itens"][0]["r"]) == ("oi claw'd", "miau, oi!")
    assert c.post("/admin/config", headers={"X-Device-Key": K},
                  json={"persona_id": "sarcastico"}).status_code == 200

    # 3) outra chave NÃO enxerga os dados desse device (isolamento por hash)
    K2 = "ffffffffffffffffffffffffffffffff"
    assert c.get("/admin/status", headers={"X-Device-Key": K2}).json()["conversas"] == 0

    # 4) sem chave ou chave fraca = barrado
    assert c.get("/admin/status").status_code == 401
    assert c.get("/admin/status", headers={"X-Device-Key": "curta"}).status_code == 401


def test_e2e_chave_anthropic_pelo_dashboard(tmp_path, monkeypatch):
    # define a chave da API pelo dashboard e confirma que o pet passa a falar
    # com ela (e que ela fica salva, mas NUNCA volta crua pro navegador)
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.delenv("ANTHROPIC_API_KEY", raising=False)
    visto = {}

    def chat_spy(cfg, system, msgs):
        visto["key"] = cfg["anthropic_api_key"]      # o que o cliente usaria
        return ("oi!", 1, 1)

    voz = VozService(stt_fn=lambda *a: "oi", chat_fn=chat_spy,
                     memoria=MemoriaService(base_dir=tmp_path))
    c = TestClient(create_app(device_keys="*", voz=voz))
    K = "a" * 32

    # 1) define a chave pelo dashboard
    assert c.post("/admin/config", headers={"X-Device-Key": K},
                  json={"anthropic_api_key": "sk-ant-minha"}).status_code == 200
    # 2) dashboard mostra "configurada", nunca a chave crua
    cfg = c.get("/admin/config", headers={"X-Device-Key": K}).json()
    assert cfg["tem_anthropic_key"] is True and "anthropic_api_key" not in cfg
    # 3) o espnokia fala -> usa exatamente a chave definida
    c.post("/claude/voz", content=b"\x00" * 40, headers={"X-Device-Key": K})
    assert visto["key"] == "sk-ant-minha"
    # 4) ficou salva no config.json (vale enquanto o server roda)
    salvo = json.loads((tmp_path / "config.json").read_text(encoding="utf-8"))
    assert salvo["anthropic_api_key"] == "sk-ant-minha"
    # 5) campo vazio NÃO apaga a chave já configurada
    c.post("/admin/config", headers={"X-Device-Key": K}, json={"anthropic_api_key": ""})
    salvo = json.loads((tmp_path / "config.json").read_text(encoding="utf-8"))
    assert salvo["anthropic_api_key"] == "sk-ant-minha"


def test_uso_logado_com_custo_por_modelo(tmp_path, monkeypatch):
    # chat_ok devolve (texto, 42 in, 17 out); modelo default = haiku (1/5 MTok)
    client, _ = faz_client(tmp_path, monkeypatch)
    assert post(client).status_code == 200
    linhas = (tmp_path / "uso.jsonl").read_text().splitlines()
    assert len(linhas) == 1
    r = json.loads(linhas[0])
    assert r["tokens_in"] == 42 and r["tokens_out"] == 17
    assert r["buscas"] == 0
    assert r["device"] == config.id8("k1")          # hash, nunca a chave crua
    assert r["custo_usd"] == round(42/1e6 + 17*5/1e6, 6)


def test_uso_soma_custo_do_web_search(tmp_path, monkeypatch):
    def chat_busca(cfg, system, mensagens):
        return "achei!", 10, 20, 2                   # 2 buscas
    client, _ = faz_client(tmp_path, monkeypatch, chat_fn=chat_busca)
    assert post(client).status_code == 200
    r = json.loads((tmp_path / "uso.jsonl").read_text().splitlines()[0])
    assert r["buscas"] == 2
    esperado = round(10/1e6 + 20*5/1e6 + 2*config.PRECO_BUSCA, 6)
    assert r["custo_usd"] == esperado
