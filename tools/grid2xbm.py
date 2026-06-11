#!/usr/bin/env python3
"""grid2xbm.py — converte grids ASCII em arrays XBM (U8g2 drawXBMP).

Formato do arquivo de grids (.txt):

    name: icon_clock
    ........
    ..####..
    .#....#.
    ........

    name: icon_clock_flipped
    from: icon_clock
    transform: rot180

- '#' = pixel aceso, '.' = apagado; largura = linha mais longa, altura = nº de linhas
- blocos separados por linha em branco; '//' inicia comentário
- transform: rot180 | flip_h | flip_v (exige 'from:')

Uso:
    python3 tools/grid2xbm.py firmware/assets/assets.txt \
        --header firmware/src/ui/assets.h [--preview /tmp/preview.png]
"""
import argparse
import sys


def parse_grids(path):
    grids = {}  # name -> list[str]
    pending = []  # linhas do bloco atual
    meta = {}

    def flush():
        if not meta.get("name"):
            return
        name = meta["name"]
        if "from" in meta:
            src = grids.get(meta["from"])
            if src is None:
                sys.exit(f"erro: '{name}' referencia '{meta['from']}' inexistente")
            rows = apply_transform(src, meta.get("transform", ""))
        else:
            rows = list(pending)
            if not rows:
                sys.exit(f"erro: grid '{name}' vazio")
        w = max(len(r) for r in rows)
        grids[name] = [r.ljust(w, ".") for r in rows]

    with open(path) as f:
        for raw in f:
            line = raw.split("//")[0].rstrip("\n")
            if not line.strip():
                flush()
                pending, meta = [], {}
                continue
            if ":" in line and not set(line.strip()) <= set("#.:"):
                key, _, val = line.partition(":")
                meta[key.strip()] = val.strip()
            else:
                pending.append(line.strip())
    flush()
    return grids


def apply_transform(rows, transform):
    if transform == "rot180":
        return [r[::-1] for r in reversed(rows)]
    if transform == "flip_h":
        return [r[::-1] for r in rows]
    if transform == "flip_v":
        return list(reversed(rows))
    sys.exit(f"erro: transform desconhecido '{transform}'")


def to_xbm(rows):
    """Empacota linhas em bytes XBM (LSB = pixel mais a esquerda)."""
    w, h = len(rows[0]), len(rows)
    data = []
    for row in rows:
        for byte_i in range((w + 7) // 8):
            b = 0
            for bit in range(8):
                x = byte_i * 8 + bit
                if x < w and row[x] == "#":
                    b |= 1 << bit
            data.append(b)
    return w, h, data


def emit_header(grids, out_path):
    lines = [
        "#pragma once",
        "// GERADO por tools/grid2xbm.py — edite os grids em firmware/assets/ e regenere.",
        "#include <U8g2lib.h>",
        "",
    ]
    for name, rows in grids.items():
        w, h, data = to_xbm(rows)
        lines.append(f"#define {name.upper()}_W {w}")
        lines.append(f"#define {name.upper()}_H {h}")
        lines.append(f"static const unsigned char {name}_bits[] U8X8_PROGMEM = {{")
        for i in range(0, len(data), 12):
            chunk = ", ".join(f"0x{b:02x}" for b in data[i : i + 12])
            lines.append(f"    {chunk},")
        lines.append("};")
        lines.append("")
    with open(out_path, "w") as f:
        f.write("\n".join(lines))


def emit_preview(grids, out_path, scale=6, pad=8):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        sys.exit("erro: --preview precisa do Pillow (pip install pillow)")

    items = list(grids.items())
    label_h = 10
    cell_w = max(len(r[0]) for _, r in items) * scale + pad
    cell_h = max(len(r) for _, r in items) * scale + pad + label_h
    cols = min(4, len(items))
    rows_n = (len(items) + cols - 1) // cols
    img = Image.new("RGB", (cols * cell_w, rows_n * cell_h), (199, 240, 216))  # verde 5110
    draw = ImageDraw.Draw(img)
    for idx, (name, rows) in enumerate(items):
        ox = (idx % cols) * cell_w + pad // 2
        oy = (idx // cols) * cell_h + pad // 2
        draw.text((ox, oy), name, fill=(60, 60, 60))
        oy += label_h
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                if ch == "#":
                    draw.rectangle(
                        [ox + x * scale, oy + y * scale, ox + x * scale + scale - 1, oy + y * scale + scale - 1],
                        fill=(20, 30, 25),
                    )
    img.save(out_path)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("grids", nargs="+", help="arquivos .txt com grids")
    ap.add_argument("--header", required=True, help="header C de saida")
    ap.add_argument("--preview", help="PNG de preview ampliado (opcional)")
    args = ap.parse_args()

    grids = {}
    for path in args.grids:
        grids.update(parse_grids(path))
    emit_header(grids, args.header)
    print(f"{len(grids)} assets -> {args.header}")
    if args.preview:
        emit_preview(grids, args.preview)
        print(f"preview -> {args.preview}")


if __name__ == "__main__":
    main()
