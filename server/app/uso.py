"""Agrega o log de uso (data/uso.jsonl) por device pro dashboard de custo.
Uma linha por chamada do Claude: {ts, device(id8), tokens_in, tokens_out,
buscas, custo_usd}. Lê tudo, filtra o device logado, soma e monta a série
diária. Escala hobby — arquivo pequeno; se um dia crescer, rotacionar por mês
(o total geral depende do histórico inteiro, então nada de cap silencioso)."""
import json
import time

from app import config


def agrega(device, now_fn=time.time, dias=30):
    alvo = config.id8(device)
    arq = config.data_dir() / "uso.jsonl"
    total_usd = mes_usd = 0.0
    tokens_in = tokens_out = falas = 0
    por_dia = {}                      # "dd/mm" -> {"usd", "falas", "_ts"}
    mes_atual = time.strftime("%Y-%m", time.localtime(now_fn()))
    if arq.exists():
        with open(arq) as f:
            for ln in f:
                try:
                    r = json.loads(ln)
                except (json.JSONDecodeError, ValueError):
                    continue          # linha corrompida / meia-escrita: ignora
                if r.get("device") != alvo:
                    continue
                usd = float(r.get("custo_usd", 0) or 0)
                ts = int(r.get("ts", 0) or 0)
                total_usd += usd
                tokens_in += int(r.get("tokens_in", 0) or 0)
                tokens_out += int(r.get("tokens_out", 0) or 0)
                falas += 1
                if time.strftime("%Y-%m", time.localtime(ts)) == mes_atual:
                    mes_usd += usd
                dia = time.strftime("%d/%m", time.localtime(ts))
                b = por_dia.setdefault(dia, {"usd": 0.0, "falas": 0, "_ts": ts})
                b["usd"] += usd
                b["falas"] += 1
                b["_ts"] = max(b["_ts"], ts)
    serie = sorted(por_dia.values(), key=lambda b: b["_ts"])[-dias:]
    orc = float(config.load().get("orcamento_usd_mes", 0) or 0)
    return {
        "mes_usd": round(mes_usd, 6),
        "total_usd": round(total_usd, 6),
        "falas": falas,
        "tokens_in": tokens_in,
        "tokens_out": tokens_out,
        "medio_usd": round(total_usd / falas, 6) if falas else 0.0,
        "serie": [{"d": time.strftime("%d/%m", time.localtime(b["_ts"])),
                   "usd": round(b["usd"], 6), "falas": b["falas"]}
                  for b in serie],
        "orcamento_usd_mes": orc,
        "mes_pct": round(mes_usd / orc, 4) if orc > 0 else None,
    }
