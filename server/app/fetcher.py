import time

import httpx


class CachedFetcher:
    """GET JSON com cache em memória; em erro de upstream serve o último bom."""

    def __init__(self, url: str, ttl_s: int, *, transport=None, clock=time.monotonic):
        self.url, self.ttl_s, self.clock = url, ttl_s, clock
        self._client = httpx.Client(transport=transport, timeout=5.0)
        self._data = None
        self._at = 0.0

    def get(self):
        """Retorna (json, idade_s)."""
        age = self.clock() - self._at
        if self._data is not None and age < self.ttl_s:
            return self._data, int(age)
        try:
            r = self._client.get(self.url)
            r.raise_for_status()
            self._data, self._at = r.json(), self.clock()
            return self._data, 0
        except Exception:
            if self._data is not None:
                return self._data, int(age)  # dados antigos > nada
            raise
