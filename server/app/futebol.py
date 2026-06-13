from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timedelta, timezone

# Brasília é UTC-3 fixo desde 2019 (sem horário de verão)
BRT = timezone(timedelta(hours=-3))

N_MAX = 8           # o buffer do aparelho segura 8 jogos por tela
JANELA_FIM_H = 4    # pós-jogo recente continua no /live: gol de acréscimo conta

# Catálogo de candidatas: o aparelho só vê liga com jogo na janela (semana
# pra trás até amanhã) — Copa do Brasil e Libertadores entram sozinhas
# quando a temporada abre.
LIGAS = [
    ("bra.1", "Brasileirão"),
    ("uefa.champions", "Champions"),
    ("bra.copa_do_brasil", "Copa do Brasil"),
    ("conmebol.libertadores", "Libertadores"),
    ("eng.1", "Premier League"),
    ("esp.1", "LaLiga"),
    ("ita.1", "Serie A"),
    ("ger.1", "Bundesliga"),
    ("fra.1", "Ligue 1"),
]


class FutebolService:
    """Agenda e placar das ligas de clube. A lista de ligas é dinâmica:
    só aparece a que tem jogo na janela — o cardápio acompanha o que está
    rolando no momento, sem mexer em firmware nem config."""

    def __init__(self, fontes, *, now_fn=None):
        self.fontes = fontes          # {liga_id: fonte com partidas()}
        self.now_fn = now_fn or (lambda: datetime.now(timezone.utc))

    def ligas(self):
        # Varre as candidatas em paralelo (cada fonte cacheia o scoreboard,
        # então a varredura repetida custa zero rede); liga quebrada só
        # fica de fora — nunca derruba o cardápio inteiro.
        def pega(fonte):
            try:
                return fonte.partidas()
            except Exception:
                return None

        with ThreadPoolExecutor(max(len(self.fontes), 1)) as ex:
            partidas = dict(zip(self.fontes,
                                ex.map(pega, self.fontes.values())))
        agora = self.now_fn()
        out = []
        for lid, nome in LIGAS:
            ps = partidas.get(lid)
            if not ps:
                continue
            liga = {"id": lid, "n": nome}
            if any(p["rolando"] for p in ps):
                liga["live"] = True       # tem bola rolando agora
            elif all(p["data"] < agora for p in ps):
                # liga parada na janela: o aparelho mostra quando foi o
                # último jogo (em Brasília, mesmo fuso do resto do payload)
                d = max(p["data"] for p in ps).astimezone(BRT)
                liga["dia"], liga["mes"] = d.day, d.month
            out.append(liga)
        return out

    def jogos(self, liga):
        fonte = self.fontes.get(liga)
        if fonte is None:
            return []
        ps = fonte.partidas()
        if len(ps) > N_MAX:
            # corta pra 8 sem nunca derrubar jogo rolando; entre os parados,
            # ficam os mais próximos de agora (recém-acabados e por vir)
            agora = self.now_fn()
            ps = sorted(ps, key=lambda p: (not p["rolando"],
                                           abs(p["data"] - agora)))[:N_MAX]
            ps.sort(key=lambda p: p["data"])
        return ps

    def live(self, liga):
        # fim recente continua aqui: o vigia de gols ainda pega o placar
        # final (gol nos acréscimos não escapa)
        corte = self.now_fn() - timedelta(hours=JANELA_FIM_H)
        return [p for p in self.jogos(liga)
                if p["rolando"] or (p["fim"] and p["data"] >= corte)]


def payload(ps, info):
    """Mesmo contrato do /copa/*: o aparelho parseia os dois apps com o
    mesmo código. Campos extras (nome do clube, minuto, gols) só quando
    têm conteúdo — payload enxuto pro ESP32."""
    jogos = []
    for p in ps:
        d = p["data"].astimezone(BRT)
        comecou = p["rolando"] or p["fim"]
        j = {"dia": d.day, "mes": d.month, "h": d.hour, "m": d.minute,
             "t1": p["t1"], "t2": p["t2"], "info": info,
             "s1": p["s1"] if comecou else -1,
             "s2": p["s2"] if comecou else -1,
             "live": p["rolando"]}
        for k in ("n1", "n2", "est"):
            if p[k]:
                j[k] = p[k]
        if p["rolando"] and p["min"]:
            j["min"] = p["min"]
        if comecou:
            if p["g1"]:
                j["g1"] = "\n".join(p["g1"])
            if p["g2"]:
                j["g2"] = "\n".join(p["g2"])
        jogos.append(j)
    return {"jogos": jogos, "atualizado_s": 0}
