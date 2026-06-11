import re
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from zoneinfo import ZoneInfo

from app.teams import code

BRT = ZoneInfo("America/Sao_Paulo")
URL = "https://raw.githubusercontent.com/openfootball/worldcup.json/master/2026/worldcup.json"


@dataclass
class Match:
    epoch: int                            # kickoff em UTC
    dia: int; mes: int; h: int; m: int    # kickoff em hora de Brasília
    t1: str; t2: str                      # códigos FIFA (ou placeholder do mata-mata)
    info: str                             # "Grupo C" / "Round of 32" / "Final"
    s1: int = -1; s2: int = -1            # -1 = sem placar


def _kickoff(date_s: str, time_s: str) -> datetime:
    # formato do openfootball: "18:00 UTC-4", "13:00 UTC-6"
    mt = re.fullmatch(r"(\d{2}):(\d{2}) UTC([+-]\d+)", time_s)
    hh, mm, off = int(mt[1]), int(mt[2]), int(mt[3])
    naive = datetime.strptime(date_s, "%Y-%m-%d").replace(hour=hh, minute=mm)
    return naive.replace(tzinfo=timezone(timedelta(hours=off)))


def _score(m: dict, key: str) -> int:
    v = m.get(key)
    return int(v) if v is not None else -1


def parse_matches(data: dict) -> list[Match]:
    out = []
    for m in data["matches"]:
        ko = _kickoff(m["date"], m["time"])
        loc = ko.astimezone(BRT)
        grupo = m.get("group")
        info = grupo.replace("Group", "Grupo") if grupo else m.get("round", "")
        out.append(Match(
            epoch=int(ko.timestamp()),
            dia=loc.day, mes=loc.month, h=loc.hour, m=loc.minute,
            t1=code(m["team1"]), t2=code(m["team2"]), info=info,
            s1=_score(m, "score1"), s2=_score(m, "score2")))
    out.sort(key=lambda x: x.epoch)
    return out
