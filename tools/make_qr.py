#!/usr/bin/env python3
"""
Gera o QR code para instalar o Kasumi Old pelo FBI (Remote Install > Scan QR Code).

Uso:  python3 tools/make_qr.py USUARIO_GITHUB [REPOSITORIO] [saida.png]
      (padrao: repositorio kasumi-old, saida docs/qr-fbi.png)

Requer: pip install opencv-python pillow
"""
import sys

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont


def make(url, out):
    params = cv2.QRCodeEncoder_Params()
    params.correction_level = cv2.QRCodeEncoder_CORRECT_LEVEL_M
    qr = cv2.QRCodeEncoder.create(params).encode(url)          # 1 px por modulo, sem margem
    mod = 8
    qr = cv2.resize(qr, None, fx=mod, fy=mod, interpolation=cv2.INTER_NEAREST)
    qr = cv2.copyMakeBorder(qr, 4 * mod, 4 * mod, 4 * mod, 4 * mod, cv2.BORDER_CONSTANT, value=255)

    # confere lendo de volta antes de salvar
    data, _, _ = cv2.QRCodeDetector().detectAndDecode(qr)
    if data != url:
        raise SystemExit(f"QR nao confere ao ler de volta: {data!r}")

    q = Image.fromarray(qr).convert("RGB")
    W = q.width + 80
    card = Image.new("RGB", (W, q.height + 150), "white")
    card.paste(q, (40, 70))
    d = ImageDraw.Draw(card)
    try:
        f1 = ImageFont.truetype("DejaVuSans-Bold.ttf", 28)
        f2 = ImageFont.truetype("DejaVuSans.ttf", 15)
    except OSError:
        f1 = f2 = ImageFont.load_default()
    d.text((W // 2, 35), "Kasumi Old  ·  FBI", font=f1, fill=(30, 30, 36), anchor="mm")
    d.text((W // 2, q.height + 90), "FBI > Remote Install > Scan QR Code", font=f2, fill=(60, 60, 70), anchor="mm")
    d.text((W // 2, q.height + 118), url, font=f2, fill=(120, 120, 130), anchor="mm")
    card.save(out)
    return data


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    user = sys.argv[1]
    repo = sys.argv[2] if len(sys.argv) > 2 else "kasumi-old"
    out = sys.argv[3] if len(sys.argv) > 3 else "docs/qr-fbi.png"
    url = f"https://github.com/{user}/{repo}/releases/latest/download/kasumi-old.cia"
    print("QR ok:", make(url, out), "->", out)
