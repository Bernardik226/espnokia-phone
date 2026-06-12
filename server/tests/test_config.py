import json

from app import config


def test_primeira_carga_cria_config_semeado_do_env(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.setenv("ANTHROPIC_API_KEY", "sk-teste")
    cfg = config.load()
    assert cfg["anthropic_api_key"] == "sk-teste"
    assert cfg["claude_model"] == "claude-haiku-4-5-20251001"
    assert cfg["stt"] == "local"
    assert cfg["max_resposta_chars"] == 220
    salvo = json.loads((tmp_path / "config.json").read_text())
    assert salvo["anthropic_api_key"] == "sk-teste"


def test_sem_env_semeia_vazio(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.delenv("ANTHROPIC_API_KEY", raising=False)
    assert config.load()["anthropic_api_key"] == ""


def test_edicao_no_arquivo_vale_na_proxima_leitura(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.delenv("ANTHROPIC_API_KEY", raising=False)
    config.load()
    arq = tmp_path / "config.json"
    cfg = json.loads(arq.read_text())
    cfg["anthropic_api_key"] = "sk-editada"
    arq.write_text(json.dumps(cfg))
    assert config.load()["anthropic_api_key"] == "sk-editada"


def test_chave_faltando_no_arquivo_ganha_default(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    (tmp_path / "config.json").write_text('{"anthropic_api_key": "k"}')
    cfg = config.load()
    assert cfg["max_resposta_chars"] == 220
    assert cfg["persona"]
