import json

from app import config


def test_primeira_carga_cria_config_semeado_do_env(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.setenv("ANTHROPIC_API_KEY", "sk-teste")
    monkeypatch.delenv("GROQ_API_KEY", raising=False)
    monkeypatch.delenv("STT_BACKEND", raising=False)
    cfg = config.load()
    assert cfg["anthropic_api_key"] == "sk-teste"
    assert cfg["claude_model"] == "claude-haiku-4-5-20251001"
    assert cfg["stt"] == "local"
    assert cfg["max_resposta_chars"] == 220
    salvo = json.loads((tmp_path / "config.json").read_text())
    assert salvo["anthropic_api_key"] == "sk-teste"


def test_chave_groq_no_env_vira_backend_default(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.setenv("GROQ_API_KEY", "gsk-teste")
    monkeypatch.delenv("STT_BACKEND", raising=False)
    cfg = config.load()
    assert cfg["stt"] == "groq"
    assert cfg["stt_api_key"] == "gsk-teste"


def test_stt_backend_no_env_ganha_da_chave(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.setenv("GROQ_API_KEY", "gsk-teste")
    monkeypatch.setenv("STT_BACKEND", "local")
    assert config.load()["stt"] == "local"


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
    assert cfg["persona_id"] == config.PERSONA_DEFAULT


def test_save_funde_campos_e_preserva_o_resto(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.setenv("ANTHROPIC_API_KEY", "sk-x")
    monkeypatch.delenv("GROQ_API_KEY", raising=False)
    monkeypatch.delenv("STT_BACKEND", raising=False)
    config.load()                       # cria o arquivo semeado
    novo = config.save({"persona_id": "sarcastico", "max_resposta_chars": 100})
    assert novo["persona_id"] == "sarcastico"
    assert novo["max_resposta_chars"] == 100
    assert novo["anthropic_api_key"] == "sk-x"   # o resto fica intacto
    salvo = json.loads((tmp_path / "config.json").read_text(encoding="utf-8"))
    assert salvo["persona_id"] == "sarcastico"


def test_persona_prompt_resolve_o_traco_da_personalidade():
    p = config.persona_prompt({"persona_id": "sarcastico"})
    assert "Claw'd" in p and "debochado" in p
    # id desconhecido cai no default, sem quebrar
    assert (config.persona_prompt({"persona_id": "xyz"}) ==
            config.persona_prompt({"persona_id": config.PERSONA_DEFAULT}))


def test_save_ignora_chaves_de_fora_do_contrato(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.delenv("GROQ_API_KEY", raising=False)
    monkeypatch.delenv("STT_BACKEND", raising=False)
    config.load()
    novo = config.save({"hacker": "rm -rf", "stt": "groq"})
    assert "hacker" not in novo
    assert novo["stt"] == "groq"


def test_id8_estavel_e_curto():
    a = config.id8("chave-aaaa")
    assert a == config.id8("chave-aaaa") and len(a) == 8
    assert a != config.id8("chave-bbbb")


def test_preco_de_por_familia():
    # USD/token = (USD/MTok) / 1_000_000
    assert config.preco_de("claude-haiku-4-5-20251001") == (1/1e6, 5/1e6)
    assert config.preco_de("claude-3-5-sonnet") == (3/1e6, 15/1e6)
    assert config.preco_de("claude-opus-4-8") == (15/1e6, 75/1e6)


def test_preco_de_case_insensitive_e_fallback():
    assert config.preco_de("Claude-HAIKU-4") == (1/1e6, 5/1e6)
    assert config.preco_de("modelo-desconhecido") == (1/1e6, 5/1e6)  # fallback haiku
    assert config.preco_de("") == (1/1e6, 5/1e6)


def test_orcamento_default_zero(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.delenv("ANTHROPIC_API_KEY", raising=False)
    assert config.load()["orcamento_usd_mes"] == 0.0
