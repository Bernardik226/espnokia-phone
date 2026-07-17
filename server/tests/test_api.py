import json
import pathlib
from datetime import datetime, timezone

from fastapi.testclient import TestClient

from app.copa import CopaService
from app.futebol import FutebolService
from app.main import create_app

FIX = pathlib.Path(__file__).parent / "fixtures" / "worldcup_sample.json"

ANTES_DA_COPA = 1_781_000_000        # 2026-06-09, nenhum jogo começou
ABERTURA_ROLANDO = 1_781_204_400 + 600   # 10 min depois de MEX x RSA


class FetcherFixture:
    def get(self):
        return json.loads(FIX.read_text()), 42


class LiveScoresFake:
    def __init__(self, tabela=None, grupos=None):
        self.tabela = tabela or {}
        self.tabela_grupos = grupos or []

    def jogos(self):
        return self.tabela

    def grupos(self):
        return self.tabela_grupos


def vivo(s1, s2, rolando=True, fim=False, minuto="", g1=None, g2=None, est=""):
    """Atalho pro shape que LiveScores.jogos() devolve por jogo."""
    return {"s1": s1, "s2": s2, "rolando": rolando, "fim": fim,
            "min": minuto, "g1": g1 or [], "g2": g2 or [], "est": est}


class FonteFutFake:
    def __init__(self, ps=None, grupos=None):
        self.ps = ps or []
        self._grupos = grupos or []

    def partidas(self):
        return self.ps

    def grupos(self):
        return self._grupos


def monta(device_keys="", agora=ANTES_DA_COPA, placares=None, grupos=None,
          futebol=None):
    svc = CopaService(FetcherFixture(), now_fn=lambda: agora)
    app = create_app(copa_service=svc,
                     live_scores=LiveScoresFake(placares, grupos),
                     device_keys=device_keys,
                     futebol=futebol or FutebolService({}))
    return TestClient(app)


def test_401_sem_chave():
    c = monta(device_keys="segredo")
    assert c.get("/copa/proximos").status_code == 401


def test_200_com_chave():
    c = monta(device_keys="segredo,outra")
    r = c.get("/copa/proximos", headers={"X-Device-Key": "outra"})
    assert r.status_code == 200


def test_health_sempre_aberto():
    c = monta(device_keys="segredo")
    assert c.get("/health").status_code == 200


def test_sem_keys_auth_desligada():
    c = monta(device_keys="")
    assert c.get("/copa/proximos").status_code == 200


def test_shape_do_contrato():
    c = monta()
    body = c.get("/copa/proximos?n=8").json()
    assert body["atualizado_s"] == 42
    j = body["jogos"][0]                       # abertura MEX x RSA, 16:00 BRT
    assert j == {"dia": 11, "mes": 6, "h": 16, "m": 0, "t1": "MEX", "t2": "RSA",
                 "info": "Grupo A", "s1": -1, "s2": -1, "live": False}


def test_n_limita_resposta():
    c = monta()
    assert len(c.get("/copa/proximos?n=2").json()["jogos"]) == 2


def test_brasil_so_jogos_do_brasil():
    c = monta()
    jogos = c.get("/copa/brasil").json()["jogos"]
    assert jogos and all("BRA" in (j["t1"], j["t2"]) for j in jogos)


def test_live_enriquece_placar():
    c = monta(agora=ABERTURA_ROLANDO, placares={("MEX", "RSA"): vivo(1, 0)})
    jogos = c.get("/copa/live").json()["jogos"]
    assert len(jogos) == 1
    assert jogos[0]["t1"] == "MEX" and jogos[0]["live"] is True
    assert (jogos[0]["s1"], jogos[0]["s2"]) == (1, 0)


def test_live_leva_minuto_gols_e_estadio():
    c = monta(agora=ABERTURA_ROLANDO, placares={("MEX", "RSA"): vivo(
        2, 0, minuto="67", g1=["J. Quiñones 9'", "R. Jiménez 67'"],
        est="Estadio Azteca · Mexico City")})
    j = c.get("/copa/live").json()["jogos"][0]
    assert (j["s1"], j["s2"]) == (2, 0)
    assert j["min"] == "67"
    assert j["g1"] == "J. Quiñones 9'\nR. Jiménez 67'"
    assert "g2" not in j                     # sem gol → sem chave → payload enxuto
    assert j["est"] == "Estadio Azteca · Mexico City"


def test_estadio_aparece_em_jogo_futuro():
    c = monta(placares={("MEX", "RSA"): vivo(
        0, 0, rolando=False, est="Estadio Azteca · Mexico City")})
    j = c.get("/copa/proximos?n=1").json()["jogos"][0]
    assert j["est"] == "Estadio Azteca · Mexico City"
    assert "min" not in j and "g1" not in j
    assert (j["s1"], j["s2"]) == (-1, -1)    # não-live: placar da tabela intacto


def test_fim_na_janela_segura_placar_da_fonte():
    # Jogo acabou mas a tabela openfootball ainda não atualizou: dentro da
    # janela live, vale o placar final da fonte ao vivo.
    c = monta(agora=ABERTURA_ROLANDO,
              placares={("MEX", "RSA"): vivo(2, 0, rolando=False, fim=True)})
    j = c.get("/copa/live").json()["jogos"][0]
    assert (j["s1"], j["s2"]) == (2, 0)
    assert "min" not in j                    # acabou: minuto não faz sentido


def test_grupos_shape_do_contrato():
    c = monta(grupos=[{"n": "A", "t": [{"c": "MEX", "pts": 3, "j": 1, "sg": 2}]}])
    body = c.get("/copa/grupos").json()
    assert body == {"grupos": [{"n": "A",
                                "t": [{"c": "MEX", "p": 3, "j": 1, "s": 2}]}]}


def test_grupos_exige_chave():
    c = monta(device_keys="segredo")
    assert c.get("/copa/grupos").status_code == 401


def fut_partida(t1, t2, rolando=True):
    return {"data": datetime(2026, 6, 12, 20, 0, tzinfo=timezone.utc),
            "t1": t1, "t2": t2, "n1": "Flamengo", "n2": "Vasco",
            "s1": 1, "s2": 0, "rolando": rolando, "fim": False,
            "min": "30", "g1": ["Pedro 12'"], "g2": [], "est": ""}


def test_futebol_ligas_e_jogos():
    fut = FutebolService({"bra.1": FonteFutFake([fut_partida("FLA", "VAS")])})
    c = monta(futebol=fut)
    assert c.get("/futebol/ligas").json() == \
        {"ligas": [{"id": "bra.1", "n": "Brasileirão", "live": True}]}
    j = c.get("/futebol/jogos", params={"liga": "bra.1"}).json()["jogos"][0]
    assert (j["t1"], j["n1"], j["s1"], j["min"]) == ("FLA", "Flamengo", 1, "30")
    assert j["info"] == "Brasileirão"   # o detail mostra de que liga é o jogo
    assert (j["h"], j["m"]) == (17, 0)  # 20:00 UTC em hora de Brasília


def test_futebol_live_so_jogo_quente():
    fut = FutebolService({"bra.1": FonteFutFake([
        fut_partida("FLA", "VAS"),
        fut_partida("SAO", "SAN", rolando=False)])},
        now_fn=lambda: datetime(2026, 6, 12, 20, 30, tzinfo=timezone.utc))
    c = monta(futebol=fut)
    jogos = c.get("/futebol/live", params={"liga": "bra.1"}).json()["jogos"]
    assert [j["t1"] for j in jogos] == ["FLA"]


def test_futebol_tabela_pontos_corridos():
    g = [{"n": "2026", "t": [{"c": "FLA", "pts": 9, "j": 3, "sg": 5}]}]
    fut = FutebolService({"bra.1": FonteFutFake(grupos=g)})
    c = monta(futebol=fut)
    body = c.get("/futebol/tabela", params={"liga": "bra.1"}).json()
    assert body == {"grupos": [{"n": "", "t": [
        {"c": "FLA", "p": 9, "j": 3, "s": 5}]}], "atualizado_s": 0}


def test_futebol_tabela_grupos():
    g = [{"n": "A", "t": [{"c": "FLA", "pts": 9, "j": 3, "sg": 5}]},
         {"n": "B", "t": [{"c": "BOC", "pts": 6, "j": 3, "sg": 1}]}]
    fut = FutebolService({"conmebol.libertadores": FonteFutFake(grupos=g)})
    c = monta(futebol=fut)
    grupos = c.get("/futebol/tabela",
                   params={"liga": "conmebol.libertadores"}).json()["grupos"]
    assert [x["n"] for x in grupos] == ["A", "B"]


def test_futebol_exige_chave():
    c = monta(device_keys="segredo")
    assert c.get("/futebol/ligas").status_code == 401
    assert c.get("/futebol/tabela", params={"liga": "bra.1"}).status_code == 401


def test_dashboard_html_aberto_sem_chave():
    c = monta(device_keys="segredo")        # a casca HTML não exige chave
    r = c.get("/")
    assert r.status_code == 200
    assert "espnokia" in r.text and "Claw'd" in r.text


def test_admin_status_exige_chave():
    c = monta(device_keys="segredo")
    assert c.get("/admin/status").status_code == 401


def test_admin_status_so_saude_sem_config(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.delenv("ANTHROPIC_API_KEY", raising=False)
    c = monta(device_keys="segredo")
    s = c.get("/admin/status", headers={"X-Device-Key": "segredo"}).json()
    assert s["versao"] == "0.5.0"
    assert s["conversas"] == 0 and s["resumidos"] == 0
    assert "memoria_ts" in s
    # config vive em /admin/config: status não repete
    assert "stt" not in s and "model" not in s and "tem_api_key" not in s and "web_search" not in s


def test_admin_config_nunca_vaza_a_chave_crua(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.setenv("ANTHROPIC_API_KEY", "sk-secreta")
    c = monta(device_keys="segredo")
    cfg = c.get("/admin/config", headers={"X-Device-Key": "segredo"}).json()
    assert "anthropic_api_key" not in cfg
    assert cfg["tem_anthropic_key"] is True


def test_admin_config_lista_personas_sem_vazar_o_prompt(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    c = monta(device_keys="segredo")
    cfg = c.get("/admin/config", headers={"X-Device-Key": "segredo"}).json()
    assert cfg["persona_id"] == "fofo"
    ids = {p["id"] for p in cfg["personas"]}
    assert {"fofo", "sarcastico"} <= ids
    # o combobox traz só id + nome, nunca o prompt/traço
    assert all(set(p) == {"id", "nome"} for p in cfg["personas"])


def test_admin_config_salva_persona_id_valida(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    c = monta(device_keys="segredo")
    r = c.post("/admin/config", headers={"X-Device-Key": "segredo"},
               json={"persona_id": "poeta", "max_resposta_chars": 99})
    assert r.status_code == 200
    cfg = c.get("/admin/config", headers={"X-Device-Key": "segredo"}).json()
    assert cfg["persona_id"] == "poeta" and cfg["max_resposta_chars"] == 99
    # id inexistente é ignorado (não corrompe a config)
    c.post("/admin/config", headers={"X-Device-Key": "segredo"},
           json={"persona_id": "hacker"})
    cfg = c.get("/admin/config", headers={"X-Device-Key": "segredo"}).json()
    assert cfg["persona_id"] == "poeta"


def test_memoria_limpar_exige_chave_e_responde_ok(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    c = monta(device_keys="segredo")
    assert c.post("/claude/memoria/limpar").status_code == 401
    r = c.post("/claude/memoria/limpar", headers={"X-Device-Key": "segredo"})
    assert r.status_code == 200 and r.json() == {"ok": True}


def test_capability_aceita_qualquer_chave_forte():
    c = monta(device_keys="*")
    r = c.get("/copa/proximos", headers={"X-Device-Key": "a" * 32})
    assert r.status_code == 200


def test_capability_recusa_sem_chave_ou_chave_curta():
    c = monta(device_keys="*")
    assert c.get("/copa/proximos").status_code == 401
    assert c.get("/copa/proximos",
                 headers={"X-Device-Key": "curtinha"}).status_code == 401


def test_pwa_manifest_sw_e_icone():
    c = monta(device_keys="segredo")        # PWA é aberto (a casca)
    m = c.get("/manifest.json")
    assert m.status_code == 200 and m.json()["name"] == "EspNokia Dash"
    assert c.get("/sw.js").status_code == 200
    assert c.get("/static/icon-192.png").status_code == 200


def test_admin_uso_shape_vazio(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    c = monta(device_keys="segredo")
    u = c.get("/admin/uso", headers={"X-Device-Key": "segredo"}).json()
    assert u["falas"] == 0 and u["total_usd"] == 0
    assert u["serie"] == [] and u["mes_pct"] is None
    assert u["orcamento_usd_mes"] == 0


def test_admin_uso_exige_chave():
    c = monta(device_keys="segredo")
    assert c.get("/admin/uso").status_code == 401


def test_admin_config_salva_orcamento(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    c = monta(device_keys="segredo")
    r = c.post("/admin/config", headers={"X-Device-Key": "segredo"},
               json={"orcamento_usd_mes": 5.5})
    assert r.status_code == 200
    cfg = c.get("/admin/config", headers={"X-Device-Key": "segredo"}).json()
    assert cfg["orcamento_usd_mes"] == 5.5
    # valor inválido é ignorado (não corrompe)
    c.post("/admin/config", headers={"X-Device-Key": "segredo"},
           json={"orcamento_usd_mes": -3})
    cfg = c.get("/admin/config", headers={"X-Device-Key": "segredo"}).json()
    assert cfg["orcamento_usd_mes"] == 5.5


def test_admin_config_rejeita_orcamento_bool(tmp_path, monkeypatch):
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    c = monta(device_keys="segredo")
    c.post("/admin/config", headers={"X-Device-Key": "segredo"},
           json={"orcamento_usd_mes": 5.0})
    # bool nao pode virar 1.0 (True e int em Python) — deve ser ignorado
    c.post("/admin/config", headers={"X-Device-Key": "segredo"},
           json={"orcamento_usd_mes": True})
    cfg = c.get("/admin/config", headers={"X-Device-Key": "segredo"}).json()
    assert cfg["orcamento_usd_mes"] == 5.0
