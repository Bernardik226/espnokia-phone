import os

from fastapi import Depends, FastAPI, Request
from fastapi.responses import JSONResponse

from app.auth import make_auth
from app.claude_voz import VozService
from app.copa import JANELA_S, CopaService
from app.fetcher import CachedFetcher
from app.futebol import LIGAS, FutebolService
from app.futebol import payload as fut_payload
from app.sources import openfootball
from app.sources.cadeia import CadeiaDeFontes
from app.sources.espn import EspnScores
from app.sources.livescore import LiveScores

TTL_TABELA_S = 15 * 60  # a tabela do openfootball muda pouco


def fonte_copa():
    """ESPN na frente (única que enxerga o ao vivo); se LIVE_SOURCE_URL
    apontar uma reserva, ela entra na cadeia como segunda opinião."""
    fontes = [EspnScores("fifa.world")]
    reserva = os.environ.get("LIVE_SOURCE_URL", "")
    if reserva:
        fontes.append(LiveScores(reserva))
    return CadeiaDeFontes(fontes)


def fonte_futebol():
    # 3 semanas pra trás: o app mostra os campeonatos "recentes" mesmo em
    # pausa de tabela (o corte de 8 jogos fica com os mais próximos de hoje)
    return FutebolService({lid: EspnScores(lid, dias_atras=21)
                           for lid, _ in LIGAS})


def create_app(copa_service=None, live_scores=None, device_keys=None,
               futebol=None, voz=None) -> FastAPI:
    if copa_service is None:
        copa_service = CopaService(CachedFetcher(openfootball.URL, TTL_TABELA_S))
    if live_scores is None:
        live_scores = fonte_copa()
    if futebol is None:
        futebol = fonte_futebol()
    if device_keys is None:
        device_keys = os.environ.get("DEVICE_KEYS", "")
    if voz is None:
        voz = VozService()

    app = FastAPI(title="espnokia server", docs_url=None, redoc_url=None)
    auth = Depends(make_auth(device_keys))

    def responde(matches, idade):
        agora = copa_service.now_fn()
        ao_vivo = live_scores.jogos()
        jogos = []
        for m in matches:
            live = m.epoch <= agora <= m.epoch + JANELA_S
            j = {"dia": m.dia, "mes": m.mes, "h": m.h, "m": m.m,
                 "t1": m.t1, "t2": m.t2, "info": m.info,
                 "s1": m.s1, "s2": m.s2, "live": live}
            lv = ao_vivo.get((m.t1, m.t2))
            if lv:
                # Campos extras só quando têm conteúdo: payload enxuto pro ESP32.
                if lv["est"]:
                    j["est"] = lv["est"]
                if live and (lv["rolando"] or lv["fim"]):
                    j["s1"], j["s2"] = lv["s1"], lv["s2"]
                    if lv["g1"]:
                        j["g1"] = "\n".join(lv["g1"])
                    if lv["g2"]:
                        j["g2"] = "\n".join(lv["g2"])
                if live and lv["rolando"] and lv["min"]:
                    j["min"] = lv["min"]
            jogos.append(j)
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

    @app.get("/copa/grupos", dependencies=[auth])
    def grupos():
        # Chaves de 1 letra: a classificação inteira cabe no buffer do ESP32.
        return {"grupos": [{"n": g["n"],
                            "t": [{"c": t["c"], "p": t["pts"],
                                   "j": t["j"], "s": t["sg"]} for t in g["t"]]}
                           for g in live_scores.grupos()]}

    nomes_ligas = dict(LIGAS)

    @app.get("/futebol/ligas", dependencies=[auth])
    def futebol_ligas():
        return {"ligas": futebol.ligas()}

    @app.get("/futebol/jogos", dependencies=[auth])
    def futebol_jogos(liga: str):
        return fut_payload(futebol.jogos(liga), nomes_ligas.get(liga, ""))

    @app.get("/futebol/live", dependencies=[auth])
    def futebol_live(liga: str):
        return fut_payload(futebol.live(liga), nomes_ligas.get(liga, ""))

    @app.post("/claude/voz", dependencies=[auth])
    async def claude_voz(request: Request, lang: str = "pt"):
        corpo = await request.body()
        device = request.headers.get("x-device-key", "")
        status, payload = voz.responder(device, corpo, lang)
        return JSONResponse(payload, status_code=status)

    return app


app = create_app()
