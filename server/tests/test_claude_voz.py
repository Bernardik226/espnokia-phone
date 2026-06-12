import json

from fastapi.testclient import TestClient

from app import config, stt
from app.claude_voz import VozService
from app.main import create_app


def stt_ok(pcm, lang, cfg):
    return "oi tudo bem"


def chat_ok(cfg, system, mensagens):
    return "miau! to otimo", 42, 17


def faz_client(tmp_path, monkeypatch, stt_fn=stt_ok, chat_fn=chat_ok,
               keys="k1,k2"):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.delenv("ANTHROPIC_API_KEY", raising=False)
    voz = VozService(stt_fn=stt_fn, chat_fn=chat_fn)
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


def test_uso_jsonl_appendado(tmp_path, monkeypatch):
    client, _ = faz_client(tmp_path, monkeypatch)
    post(client)
    post(client)
    linhas = (tmp_path / "uso.jsonl").read_text().strip().splitlines()
    assert len(linhas) == 2
    uso = json.loads(linhas[0])
    assert uso["device"] == "k1"
    assert uso["tokens_in"] == 42
    assert uso["tokens_out"] == 17
    assert uso["custo_usd"] > 0
