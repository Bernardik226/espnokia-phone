import time
from datetime import datetime, timedelta, timezone

import httpx

# API pública do site da ESPN: sem chave, estável há anos e cobre as ligas
# principais (Copa, Brasileirão, Champions, Libertadores...). Endpoints:
#   site/v2/sports/soccer/{liga}/scoreboard?dates=AAAAMMDD-AAAAMMDD
#   v2/sports/soccer/{liga}/standings
URL = "https://site.api.espn.com/apis"

TTL_JOGOS_S = 60
TTL_GRUPOS_S = 300


def _int(v, padrao=0):
    try:
        return int(v)
    except (TypeError, ValueError):
        return padrao


def _hoje_utc():
    return datetime.now(timezone.utc).date()


def _minuto(status):
    """displayClock "72'" → "72"; "45'+3'" → "45+3" (o aparelho repõe o ')."""
    return str(status.get("displayClock", "")).replace("'", "").strip()


def _autor(detalhe):
    """Evento de gol → "R. Jimenez 67'" (nome curto + minuto)."""
    atletas = detalhe.get("athletesInvolved") or [{}]
    nome = atletas[0].get("shortName") or atletas[0].get("displayName", "?")
    quando = detalhe.get("clock", {}).get("displayValue", "")
    return (nome + " " + quando).strip()


class EspnScores:
    """Placar ao vivo e classificação da ESPN; erro na fonte devolve o último
    cache (placar velho > placar nenhum). Mesma interface pra qualquer liga."""

    def __init__(self, liga, base_url=URL, *, transport=None,
                 clock=time.monotonic, hoje=_hoje_utc):
        self.liga = liga
        self.base_url = base_url.rstrip("/")
        self.clock = clock
        self.hoje = hoje
        self._client = httpx.Client(transport=transport, timeout=5.0)
        self._jogos = {}
        self._jogos_at = float("-inf")  # força fetch na primeira chamada
        self._grupos = []
        self._grupos_at = float("-inf")

    def _get(self, path):
        r = self._client.get(self.base_url + path)
        r.raise_for_status()
        return r.json()

    def _scoreboard(self):
        # Janela ontem→amanhã: o "dia" da ESPN é fuso dos EUA e jogo de
        # madrugada em Brasília cai no dia vizinho.
        d = self.hoje()
        faixa = "{}-{}".format((d - timedelta(days=1)).strftime("%Y%m%d"),
                               (d + timedelta(days=1)).strftime("%Y%m%d"))
        return self._get("/site/v2/sports/soccer/" + self.liga +
                         "/scoreboard?dates=" + faixa)

    def jogos(self):
        """{(t1, t2): {s1, s2, rolando, fim, min, g1, g2, est}}. Cada jogo
        entra nas DUAS ordens do par — a tabela local pode inverter mandante."""
        if self.clock() - self._jogos_at < TTL_JOGOS_S:
            return self._jogos
        try:
            eventos = self._scoreboard().get("events", [])
        except Exception:
            return self._jogos
        out = {}
        for ev in eventos:
            comp = (ev.get("competitions") or [{}])[0]
            lados = {c.get("homeAway"): c for c in comp.get("competitors", [])}
            casa, fora = lados.get("home", {}), lados.get("away", {})
            c1 = casa.get("team", {}).get("abbreviation")
            c2 = fora.get("team", {}).get("abbreviation")
            if not c1 or not c2:
                continue
            status = comp.get("status", {})
            estado = status.get("type", {}).get("state", "")
            rolando, fim = estado == "in", estado == "post"
            gols = {}  # team.id → ["autor 59'", ...], na ordem dos eventos
            for det in comp.get("details", []):
                if not det.get("scoringPlay") or det.get("shootout"):
                    continue
                gols.setdefault(det.get("team", {}).get("id"), []).append(
                    _autor(det))
            venue = comp.get("venue", {})
            est = venue.get("fullName", "")
            cidade = venue.get("address", {}).get("city", "")
            if est and cidade:
                est = est + " · " + cidade
            j = {
                "s1": _int(casa.get("score")),
                "s2": _int(fora.get("score")),
                "rolando": rolando,
                "fim": fim,
                "min": _minuto(status) if rolando else "",
                "g1": gols.get(casa.get("team", {}).get("id"), []),
                "g2": gols.get(fora.get("team", {}).get("id"), []),
                "est": est,
            }
            out[(c1, c2)] = j
            out[(c2, c1)] = {**j, "s1": j["s2"], "s2": j["s1"],
                             "g1": j["g2"], "g2": j["g1"]}
        self._jogos, self._jogos_at = out, self.clock()
        return self._jogos

    def grupos(self):
        """[{n, t: [{c, pts, j, sg}]}] na ordem oficial (rank da fonte);
        "Group A" vira "A" pro título curto do aparelho."""
        if self.clock() - self._grupos_at < TTL_GRUPOS_S:
            return self._grupos
        try:
            filhos = self._get("/v2/sports/soccer/" + self.liga +
                               "/standings").get("children", [])
        except Exception:
            return self._grupos
        out = []
        for ch in filhos:
            nome = str(ch.get("name", "?"))
            if nome.startswith("Group "):
                nome = nome[len("Group "):]
            t = []
            for e in ch.get("standings", {}).get("entries", []):
                stats = {s.get("name"): s.get("value")
                         for s in e.get("stats", [])}
                t.append({"c": e.get("team", {}).get("abbreviation", "?"),
                          "pts": _int(stats.get("points")),
                          "j": _int(stats.get("gamesPlayed")),
                          "sg": _int(stats.get("pointDifferential")),
                          "rank": _int(stats.get("rank"), 99)})
            t.sort(key=lambda x: (x["rank"], -x["pts"], -x["sg"]))
            for x in t:
                x.pop("rank")
            out.append({"n": nome, "t": t})
        out.sort(key=lambda g: g["n"])
        self._grupos, self._grupos_at = out, self.clock()
        return self._grupos
