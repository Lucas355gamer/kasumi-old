#!/usr/bin/env python3
"""
Gera fontes bitmap (alfa de 4 bits, proporcionais) para o app 3DS.

Uso:  python3 tools/genfont.py caminho/Inter-Medium.otf 3ds/source
      (gera fonts.h e fonts.c)

Cobre ASCII + Latin-1 (acentos do portugues). Requer Pillow.
"""
import sys
from PIL import Image, ImageDraw, ImageFont

SIZES = [("font_small", 11), ("font_body", 13), ("font_title", 20)]
CHARS = list(range(32, 127)) + list(range(160, 256))
# Simbolos extras nos codigos livres 0x80-0x86 (o app converte de Unicode):
#   0x80 ◄  0x81 ►  0x82 ▲  0x83 ▼  0x84 ⋯  0x85 ‹  0x86 ›
EXTRA = {0x80: "tri_l", 0x81: "tri_r", 0x82: "tri_u", 0x83: "tri_d",
         0x84: "\u2026", 0x85: "\u2039", 0x86: "\u203a"}


def build(path, name, size):
    font = ImageFont.truetype(path, size)
    ascent, descent = font.getmetrics()
    height = ascent + descent
    glyphs, data = [], bytearray()
    for cp in range(256):
        if cp not in CHARS and cp not in EXTRA:
            glyphs.append((0, 0, 0, 0))
            continue
        img = Image.new("L", (size * 3, height), 0)
        d = ImageDraw.Draw(img)
        ex = EXTRA.get(cp)
        if ex and ex.startswith("tri"):
            t = max(4, int(size * 0.55))          # lado do triangulo
            x0, y0 = size + 1, ascent - t
            pts = {"tri_l": [(x0 + t, y0), (x0 + t, y0 + t), (x0, y0 + t / 2)],
                   "tri_r": [(x0, y0), (x0, y0 + t), (x0 + t, y0 + t / 2)],
                   "tri_u": [(x0, y0 + t), (x0 + t, y0 + t), (x0 + t / 2, y0)],
                   "tri_d": [(x0, y0), (x0 + t, y0), (x0 + t / 2, y0 + t)]}[ex]
            big = img.resize((img.width * 4, img.height * 4))
            ImageDraw.Draw(big).polygon([(x * 4, y * 4) for x, y in pts], fill=255)
            img = big.resize(img.size, Image.LANCZOS)
            adv = t + 3
        else:
            ch = ex if ex else chr(cp)
            adv = int(round(font.getlength(ch)))
            d.text((size, 0), ch, font=font, fill=255)
        bbox = img.getbbox()
        if not bbox:
            glyphs.append((len(data), 0, 0, max(adv, 1)))
            continue
        x0, _, x1, _ = bbox
        w = x1 - x0
        nibbles = []
        for y in range(height):
            for x in range(x0, x1):
                nibbles.append(img.getpixel((x, y)) >> 4)
        if len(nibbles) & 1:
            nibbles.append(0)
        off = len(data)
        for i in range(0, len(nibbles), 2):
            data.append(nibbles[i] | (nibbles[i + 1] << 4))
        xoff = x0 - size  # pode ser negativo
        glyphs.append((off, w, xoff, adv))

    out = [f"static const uint8_t {name}_data[{len(data)}] = {{"]
    for i in range(0, len(data), 24):
        out.append("    " + ",".join(str(b) for b in data[i:i + 24]) + ",")
    out.append("};")
    out.append(f"static const ko_glyph_t {name}_glyphs[256] = {{")
    for g in glyphs:
        out.append(f"    {{{g[0]},{g[1]},{g[2]},{g[3]}}},")
    out.append("};")
    out.append(f"const ko_font_t {name} = {{ {height}, {ascent}, {name}_glyphs, {name}_data }};")
    return "\n".join(out)


def main():
    src, outdir = sys.argv[1], sys.argv[2]
    hdr = [
        "/* Gerado por tools/genfont.py - nao editar. Fonte: Inter (SIL Open Font License). */",
        "#ifndef KO_FONTS_H", "#define KO_FONTS_H", "#include <stdint.h>",
        "typedef struct { uint32_t offset; uint8_t w; int8_t xoff; uint8_t adv; } ko_glyph_t;",
        "typedef struct { uint8_t height; uint8_t ascent; const ko_glyph_t *glyphs; const uint8_t *data; } ko_font_t;",
    ]
    hdr += [f"extern const ko_font_t {name};" for name, _ in SIZES]
    hdr.append("#endif")
    body = ["/* Gerado por tools/genfont.py - nao editar. Fonte: Inter (SIL Open Font License). */",
            '#include "fonts.h"']
    for name, size in SIZES:
        body.append(build(src, name, size))
    with open(f"{outdir}/fonts.h", "w") as f:
        f.write("\n".join(hdr) + "\n")
    with open(f"{outdir}/fonts.c", "w") as f:
        f.write("\n".join(body) + "\n")


if __name__ == "__main__":
    main()
