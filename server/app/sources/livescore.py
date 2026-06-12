import re
import time

import httpx

from app.teams import code

# Os autores de gol vêm numa string estilo "{“J. Quiñones 9'”,”R. Jiménez 67'”}"
# — com aspas tipográficas (e às vezes retas). O regex pega o miolo de cada par.
_RE_SCORER = re.compile(r'[“”"]([^“”"]+)[“”"]')

TTL_JOGOS_S = 60
TTL_GRUPOS_S = 300


def _int(v, padrao=0):
    try:
        return int(v)
    except (TypeError, ValueError):
        return padrao


def _parse_scorers(raw):
    """String de autores da fonte → lista ["J. Quiñones 9'", ...]; "null" → []."""
    raw = str(raw or "")
    if raw.lower() in ("", "null"):
        return []
    return _RE_SCORER.findall(raw)


class LiveScores:
    """Dados ao vivo do worldcup26.ir; fonte instável → erro devolve o último cache."""

    def __init__(self, base_url, *, transport=None, clock=time.monotonic):
        self.base_url = (base_url or "").rstrip("/")
        self.clock = clock
        self._client = (httpx.Client(transport=transport, timeout=5.0)
                        if self.base_url else None)
        self._jogos = {}
        self._jogos_at = float("-inf")  # força fetch na primeira chamada
        self._grupos = []
        self._grupos_at = float("-inf")
        self._estadios = None  # id → "Nome · Cidade"; None = ainda não buscou
        self._times = None  # id → código FIFA

    def _get(self, path):
        r = self._client.get(self.base_url + path)
        r.raise_for_status()
        return r.json()

    # Estádios e times mudam zero durante a Copa: um fetch resolve.
    # Se falhar, fica None e tenta de novo no próximo refresh.

    def _carrega_estadios(self):
        if self._estadios is None:
            try:
                self._estadios = {s["id"]: s["name_en"] + " · " + s["city_en"]
                                  for s in self._get("/get/stadiums")["stadiums"]}
            except Exception:
                pass
        return self._estadios or {}

    def _carrega_times(self):
        if self._times is None:
            try:
                self._times = {t["id"]: t["fifa_code"]
                               for t in self._get("/get/teams")["teams"]}
            except Exception:
                pass
        return self._times or {}

    def jogos(self):
        """{(t1, t2): {s1, s2, rolando, fim, min, g1, g2, est}} de todos os jogos
        do feed; em erro, mantém o último cache (placar velho > placar nenhum)."""
        if not self.base_url:
            return {}
        if self.clock() - self._jogos_at < TTL_JOGOS_S:
            return self._jogos
        try:
            games = self._get("/get/games")["games"]
        except Exception:
            return self._jogos
        estadios = (self._carrega_estadios()
                    if any(g.get("stadium_id") for g in games) else {})
        out = {}
        for g in games:
            t1, t2 = g.get("home_team_name_en"), g.get("away_team_name_en")
            if not t1 or not t2:
                continue  # mata-mata sem times definidos ("Runner-up Group...")
            fim = str(g.get("finished")).lower() == "true"
            rolando = (not fim
                       and str(g.get("time_elapsed")).lower() != "notstarted")
            par = (code(t1), code(t2))
            out[par] = {
                "s1": _int(g.get("home_score")),
                "s2": _int(g.get("away_score")),
                "rolando": rolando,
                "fim": fim,
                "min": str(g.get("time_elapsed", "")) if rolando else "",
                "g1": _parse_scorers(g.get("home_scorers")),
                "g2": _parse_scorers(g.get("away_scorers")),
                "est": estadios.get(g.get("stadium_id"), ""),
            }
        self._jogos, self._jogos_at = out, self.clock()
        return out

    def scores(self):
        """Compat: {(t1, t2): (s1, s2)} só dos jogos em andamento."""
        return {par: (j["s1"], j["s2"]) for par, j in self.jogos().items()
                if j["rolando"]}

    def grupos(self):
        """[{n, t: [{c, pts, j, sg}]}] com times já classificados por pontos e
        saldo; em erro, mantém o último cache."""
        if not self.base_url:
            return []
        if self.clock() - self._grupos_at < TTL_GRUPOS_S:
            return self._grupos
        try:
            grupos = self._get("/get/groups")["groups"]
        except Exception:
            return self._grupos
        times = self._carrega_times() if grupos else {}
        out = []
        for g in grupos:
            t = [{"c": times.get(x.get("team_id"), "?"),
                  "pts": _int(x.get("pts")),
                  "j": _int(x.get("mp")),
                  "sg": _int(x.get("gd"))} for x in g.get("teams", [])]
            t.sort(key=lambda x: (-x["pts"], -x["sg"]))
            out.append({"n": g.get("name", "?"), "t": t})
        out.sort(key=lambda g: g["n"])
        self._grupos, self._grupos_at = out, self.clock()
        return out
