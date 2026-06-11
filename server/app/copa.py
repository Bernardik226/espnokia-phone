import time

from app.sources.openfootball import parse_matches

JANELA_S = 130 * 60  # kickoff + 130 min = jogo "em andamento"
N_MAX = 20


class CopaService:
    """Filtra a tabela da Copa: próximos jogos, jogos do Brasil e ao vivo."""

    def __init__(self, fetcher, *, now_fn=time.time):
        self.fetcher = fetcher
        self.now_fn = now_fn
        self._parse = parse_matches

    def _matches(self):
        data, idade = self.fetcher.get()
        return sorted(self._parse(data), key=lambda m: m.epoch), idade

    def proximos(self, n: int):
        ms, idade = self._matches()
        agora = self.now_fn()
        vivos = [m for m in ms if m.epoch + JANELA_S > agora]
        return vivos[: max(1, min(n, N_MAX))], idade

    def brasil(self):
        ms, idade = self._matches()
        agora = self.now_fn()
        return [m for m in ms
                if "BRA" in (m.t1, m.t2) and m.epoch + JANELA_S > agora], idade

    def live(self):
        ms, idade = self._matches()
        agora = self.now_fn()
        return [m for m in ms if m.epoch <= agora <= m.epoch + JANELA_S], idade
