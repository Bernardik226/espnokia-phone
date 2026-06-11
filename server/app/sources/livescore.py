import time

import httpx

from app.teams import code


class LiveScores:
    """Placar ao vivo do worldcup26.ir; fonte instável → qualquer erro vira {}."""

    def __init__(self, base_url, *, transport=None, clock=time.monotonic):
        self.base_url = (base_url or "").rstrip("/")
        self.clock = clock
        self._client = (httpx.Client(transport=transport, timeout=5.0)
                        if self.base_url else None)
        self._cache = {}
        self._at = float("-inf")  # força fetch na primeira chamada

    def scores(self):
        """{(t1, t2): (s1, s2)} dos jogos em andamento; {} se desligado ou em erro."""
        if not self.base_url:
            return {}
        if self.clock() - self._at < 60:
            return self._cache
        try:
            r = self._client.get(self.base_url + "/get/games")
            r.raise_for_status()
            out = {}
            for g in r.json()["games"]:
                if str(g.get("finished")).lower() == "true":
                    continue
                if str(g.get("time_elapsed")).lower() == "notstarted":
                    continue
                t1, t2 = code(g["home_team_name_en"]), code(g["away_team_name_en"])
                out[(t1, t2)] = (int(g["home_score"]), int(g["away_score"]))
            self._cache, self._at = out, self.clock()
            return out
        except Exception:
            return {}
