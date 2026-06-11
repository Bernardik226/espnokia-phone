import os

from fastapi import Depends, FastAPI

from app.auth import make_auth
from app.copa import JANELA_S, CopaService
from app.fetcher import CachedFetcher
from app.sources import openfootball
from app.sources.livescore import LiveScores

TTL_TABELA_S = 15 * 60  # a tabela do openfootball muda pouco


def create_app(copa_service=None, live_scores=None, device_keys=None) -> FastAPI:
    if copa_service is None:
        copa_service = CopaService(CachedFetcher(openfootball.URL, TTL_TABELA_S))
    if live_scores is None:
        live_scores = LiveScores(os.environ.get("LIVE_SOURCE_URL", ""))
    if device_keys is None:
        device_keys = os.environ.get("DEVICE_KEYS", "")

    app = FastAPI(title="espnokia server", docs_url=None, redoc_url=None)
    auth = Depends(make_auth(device_keys))

    def responde(matches, idade):
        agora = copa_service.now_fn()
        placares = live_scores.scores()
        jogos = []
        for m in matches:
            live = m.epoch <= agora <= m.epoch + JANELA_S
            s1, s2 = m.s1, m.s2
            if live and (m.t1, m.t2) in placares:
                s1, s2 = placares[(m.t1, m.t2)]
            jogos.append({"dia": m.dia, "mes": m.mes, "h": m.h, "m": m.m,
                          "t1": m.t1, "t2": m.t2, "info": m.info,
                          "s1": s1, "s2": s2, "live": live})
        return {"jogos": jogos, "atualizado_s": idade}

    @app.get("/health")
    def health():
        return {"status": "ok"}

    @app.get("/copa/proximos", dependencies=[auth])
    def proximos(n: int = 8):
        return responde(*copa_service.proximos(n))

    @app.get("/copa/brasil", dependencies=[auth])
    def brasil():
        return responde(*copa_service.brasil())

    @app.get("/copa/live", dependencies=[auth])
    def live():
        return responde(*copa_service.live())

    return app


app = create_app()
