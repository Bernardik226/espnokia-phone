from fastapi import Header, HTTPException


def make_auth(device_keys_csv: str):
    """Dependency de auth por X-Device-Key. CSV vazio = auth desligada (dev)."""
    keys = {k.strip() for k in (device_keys_csv or "").split(",") if k.strip()}

    def auth(x_device_key: str | None = Header(default=None)):
        if keys and x_device_key not in keys:
            raise HTTPException(status_code=401, detail="chave invalida")

    return auth
