from fastapi import Header, HTTPException

MIN_LEN = 16   # chave de capability precisa ter entropia (a do device tem 32)


def make_auth(device_keys_csv: str):
    """Dependency de auth por X-Device-Key. Três modos pela env DEVICE_KEYS:
      ""   -> desligada (dev/teste, aceita tudo)
      "*"  -> capability: aceita qualquer chave forte (>= MIN_LEN). A chave É a
              credencial; os dados ficam isolados por hash dela. Sem lista no
              server (casa com storage efêmero) — é o modo do device que sorteia
              a própria chave e pareia por QR.
      "a,b"-> allowlist clássica: só as chaves listadas passam.
    """
    raw = (device_keys_csv or "").strip()
    capability = raw == "*"
    keys = set() if capability else {k.strip() for k in raw.split(",") if k.strip()}

    def auth(x_device_key: str | None = Header(default=None)):
        if capability:
            if not x_device_key or len(x_device_key) < MIN_LEN:
                raise HTTPException(status_code=401, detail="chave invalida")
        elif keys and x_device_key not in keys:
            raise HTTPException(status_code=401, detail="chave invalida")

    return auth
