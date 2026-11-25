#!/usr/bin/env python3
"""
Testes de ponta a ponta sem 3DS:
  - o codigo C do 3DS (reasm.c, adpcm.c) e compilado como biblioteca e usado via ctypes;
  - a ponte Python real roda por UDP em localhost, com captura sintetica;
  - um "3DS falso" descobre, conecta, remonta quadros, decodifica audio e manda botoes.

Rodar:  python3 tests/test_e2e.py
"""
import ctypes
import io
import math
import os
import random
import socket
import struct
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "3ds", "source")
sys.path.insert(0, os.path.join(ROOT, "pc"))

from PIL import Image, ImageChops, ImageDraw  # noqa: E402
import kasumi_bridge as kb  # noqa: E402

LIB = os.path.join(HERE, "libko_test.so")
subprocess.check_call(["gcc", "-O2", "-shared", "-fPIC", "-Wall", "-o", LIB,
                       os.path.join(SRC, "reasm.c"), os.path.join(SRC, "adpcm.c"), "-I", SRC])
lib = ctypes.CDLL(LIB)

REASM_SIZE = 4 * 1024 * 1024  # folga para a struct (2 x 64 KB + campos)
lib.ko_reasm_push.restype = ctypes.c_void_p
lib.ko_reasm_push.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_uint32)]
lib.ko_reasm_init.argtypes = [ctypes.c_void_p]
lib.ko_adpcm_decode.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int16, ctypes.c_int, ctypes.POINTER(ctypes.c_int16)]

failures = []


def check(cond, msg):
    print(("  ok   " if cond else "  FALHA ") + msg)
    if not cond:
        failures.append(msg)


class Reasm:
    def __init__(self):
        self.buf = ctypes.create_string_buffer(REASM_SIZE)
        lib.ko_reasm_init(self.buf)

    def push(self, payload):
        size = ctypes.c_uint32(0)
        ptr = lib.ko_reasm_push(self.buf, payload, len(payload), ctypes.byref(size))
        if not ptr:
            return None
        return ctypes.string_at(ptr, size.value)

    def counters(self):
        # frames_ok/frames_lost sao os dois ultimos uint32 da struct
        slot = 4 + 2 + 1 + 1 + 4 + 32 + 64 * 1024
        slot = (slot + 3) // 4 * 4
        off = 2 * slot + 4 + 2
        off = (off + 3) // 4 * 4
        return struct.unpack_from("<II", self.buf.raw, off)


def adpcm_decode_c(pred, idx, data, n):
    out = (ctypes.c_int16 * n)()
    lib.ko_adpcm_decode(data, n, pred, idx, out)
    return list(out)


def screen_from_jpeg(jpeg):
    """Simula o 3DS: linha r do JPEG vai para fb[r*240...]; pixel(x,y) = fb[x*240 + 239-y]."""
    im = Image.open(io.BytesIO(jpeg)).convert("RGB")
    assert im.size == (240, 400), im.size
    src = im.load()
    out = Image.new("RGB", (400, 240))
    dst = out.load()
    for x in range(400):
        for y in range(240):
            dst[x, y] = src[239 - y, x]
    return out


def test_pattern():
    """Imagem 1280x720 com marcas assimetricas para pegar rotacao/espelhamento errados."""
    im = Image.new("RGB", (1280, 720), (20, 20, 60))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 320, 180), fill=(255, 0, 0))          # canto superior esquerdo: vermelho
    d.rectangle((960, 540, 1279, 719), fill=(0, 255, 0))     # inferior direito: verde
    d.rectangle((1100, 0, 1279, 120), fill=(0, 0, 255))      # superior direito: azul
    return im


def mean_diff(a, b):
    diff = ImageChops.difference(a, b).convert("L")
    h = diff.histogram()
    return sum(i * c for i, c in enumerate(h)) / (a.size[0] * a.size[1])


# ------------------------------------------------------------------ 1. orientacao e fragmentacao
print("\n[1] Video: orientacao e remontagem no codigo C do 3DS")
src = test_pattern()
fitted = kb.fit_frame(src)
check(fitted.size == (400, 240), "fit_frame gera 400x240")
jpeg = kb.encode_jpeg(kb.to_3ds_layout(fitted), 70)
check(jpeg is not None and len(jpeg) < kb.VIDEO_MAX, f"JPEG cabe no limite ({len(jpeg)} bytes)")
shown = screen_from_jpeg(jpeg)
check(mean_diff(shown, fitted) < 6, f"imagem no 3DS bate com a original (dif media {mean_diff(shown, fitted):.2f})")
px = shown.load()
check(px[30, 30][0] > 180 and px[30, 30][1] < 80, "vermelho no canto superior esquerdo")
check(px[380, 225][1] > 180, "verde no canto inferior direito")
check(px[385, 15][2] > 180, "azul no canto superior direito")

pkts = kb.video_packets(7, jpeg)
r = Reasm()
random.seed(1)
shuffled = pkts[:]
random.shuffle(shuffled)
got = None
for p in shuffled + shuffled[:3]:            # fora de ordem + duplicados
    res = r.push(p[4:])
    if res:
        got = res
check(got == jpeg, f"quadro remontado identico ({len(pkts)} fragmentos, fora de ordem e duplicados)")

# perda: quadro 8 perde um fragmento, quadro 9 chega inteiro
p8 = kb.video_packets(8, jpeg)
p9 = kb.video_packets(9, jpeg[:-1] + b"\xd9")
for p in p8[:-2] + p8[-1:]:
    check(r.push(p[4:]) is None, "quadro incompleto nao e entregue") if p is p8[0] else r.push(p[4:])
done = [r.push(p[4:]) for p in p9]
check(done[-1] is not None, "quadro seguinte entregue apesar da perda anterior")
ok, lost = r.counters()
check((ok, lost) == (2, 1), f"contadores ok={ok} perdidos={lost} (esperado 2 e 1)")
# quadro atrasado (8 chegando depois do 9) e ignorado
check(r.push(p8[-2][4:]) is None, "fragmento atrasado de quadro velho e ignorado")
# lixo
check(r.push(b"\x00" * 3) is None and r.push(b"\xff" * 40) is None, "pacotes invalidos ignorados sem travar")
# volta do contador u16
r2 = Reasm()
a = [r2.push(p[4:]) for p in kb.video_packets(65535, jpeg)][-1]
b = [r2.push(p[4:]) for p in kb.video_packets(0, jpeg)][-1]
check(a is not None and b is not None, "id de quadro dando a volta (65535 -> 0)")

# ------------------------------------------------------------------ 2. ADPCM
print("\n[2] Audio: ADPCM do Python decodificado pelo C do 3DS")
enc = kb.AdpcmEncoder()
sig = [int(12000 * math.sin(2 * math.pi * 440 * i / 24000) + 4000 * math.sin(2 * math.pi * 1300 * i / 24000))
       for i in range(1024 * 4)]
dec = []
for blk in range(4):
    chunk = sig[blk * 1024:(blk + 1) * 1024]
    pkt = kb.audio_packet(blk, chunk, enc)
    seq, n, pred, idx, _ = struct.unpack_from("<HHhBB", pkt, 4)
    dec += adpcm_decode_c(pred, idx, pkt[12:], n)
err = sum((a - b) ** 2 for a, b in zip(sig, dec)) / len(sig)
pwr = sum(a * a for a in sig) / len(sig)
snr = 10 * math.log10(pwr / max(err, 1e-9))
check(snr > 20, f"SNR do audio decodificado: {snr:.1f} dB")
check(len(kb.audio_packet(0, sig[:1024], kb.AdpcmEncoder())) == 4 + 8 + 512, "pacote de audio com 524 bytes")

# ------------------------------------------------------------------ 3. ponta a ponta por UDP
print("\n[3] Ponta a ponta: ponte real + 3DS falso por UDP")


class FakeCapture:
    def __init__(self, *a, **k):
        self.n = 0

    def grab(self):
        self.n += 1
        im = test_pattern()
        ImageDraw.Draw(im).text((600, 340), str(self.n), fill=(255, 255, 255))
        return im


class FakeOutput:
    def __init__(self):
        self.states = []

    def apply(self, st):
        self.states.append(st)

    def neutral(self):
        self.states.append("neutral")


kb.Capture = FakeCapture
port = 39000 + random.randint(0, 500)
args = type("A", (), dict(name="PC-Teste", port=port, monitor=1, window=None, region=None,
                          stretch=False, no_audio=True, keyboard=False))()
bridge = kb.Bridge(args)
fake_out = FakeOutput()
bridge.output = fake_out
for target in (bridge.recv_loop, bridge.video_loop):
    threading.Thread(target=target, daemon=True).start()

c = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
c.bind(("127.0.0.1", 0))
c.settimeout(2.0)
dst = ("127.0.0.1", port)

c.sendto(kb.header(kb.T_DISCOVER), dst)
data, _ = c.recvfrom(2048)
check(data[2] == kb.T_ANNOUNCE and data[4:] == b"PC-Teste", "DISCOVER -> ANNOUNCE com o nome do PC")

c.sendto(kb.header(kb.T_HELLO) + bytes((15, 55, 0, 1)), dst)
data, _ = c.recvfrom(2048)
check(data[2] == kb.T_WELCOME, "HELLO -> WELCOME")

r = Reasm()
frames, t_end = [], time.time() + 2.5
while time.time() < t_end:
    try:
        data, _ = c.recvfrom(2048)
    except socket.timeout:
        break
    if data[2] == kb.T_VIDEO:
        f = r.push(data[4:])
        if f:
            frames.append(f)
    # heartbeat / botoes: A + analogico esquerdo para cima + gatilho direito
    inp = kb.header(kb.T_INPUT) + struct.pack("<IIhhhhBBhhBBH", len(frames), kb.B_A, 0, 32767, 100, -100,
                                              0, 255, 5, -3, 0, 0, 0) + b"\0\0"
    c.sendto(inp, dst)
fps = len(frames) / 2.5
check(len(frames) >= 25, f"quadros recebidos e remontados: {len(frames)} (~{fps:.0f} fps, alvo 15)")
if frames:
    shown = screen_from_jpeg(frames[-1])
    check(mean_diff(shown, kb.fit_frame(test_pattern())) < 8, "ultimo quadro bate com a captura")
check(len(fake_out.states) > 0 and isinstance(fake_out.states[-1], dict), "INPUT chega na saida do controle")
if fake_out.states and isinstance(fake_out.states[-1], dict):
    st = fake_out.states[-1]
    check(st["buttons"] == kb.B_A and st["ly"] == 32767 and st["rt"] == 255 and st["rx"] == 100,
          f"botoes/analogicos corretos ({st['buttons']}, ly={st['ly']}, rt={st['rt']}, rx={st['rx']})")

# ajuste automatico: 3DS relata decodificacao lenta (90 ms) -> ponte reduz fps
c.sendto(kb.header(kb.T_STATS) + struct.pack("<HHHH", 15, 0, 90, 0), dst)
time.sleep(0.3)
check(bridge.adapter.fps <= 9, f"ajuste automatico reduz fps quando o 3DS nao da conta ({bridge.adapter.fps} fps)")
c.sendto(kb.header(kb.T_STATS) + struct.pack("<HHHH", 10, 5, 40, 0), dst)
time.sleep(0.3)
check(bridge.adapter.quality < 55, f"perda alta reduz a qualidade JPEG ({bridge.adapter.quality})")

c.sendto(kb.header(kb.T_BYE), dst)
time.sleep(0.3)
check(bridge.client is None and fake_out.states[-1] == "neutral", "BYE encerra a sessao e solta os botoes")
bridge.running = False

print()
if failures:
    print(f"{len(failures)} FALHA(S)")
    sys.exit(1)
print("Todos os testes passaram.")
