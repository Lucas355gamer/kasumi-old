#!/usr/bin/env python3
"""
Kasumi Bridge - a ponte do PC para o Kasumi Old (Nintendo 3DS / Old 3DS).

Captura a tela (ou uma janela) e o som do PC e manda para o 3DS num formato
que ate o Old 3DS decodifica: JPEG 400x240 + audio IMA ADPCM 24 kHz, por UDP.
Os botoes do 3DS viram um controle de Xbox 360 virtual (ViGEm), ou teclado e
mouse se o controle virtual nao estiver disponivel.

Funciona com qualquer coisa que rode no PC: GeForce NOW, Xbox Cloud Gaming
(no navegador), Steam, emuladores, jogos nativos...

Uso rapido:
    pip install -r requirements.txt
    python kasumi_bridge.py                      # tela principal
    python kasumi_bridge.py --window "GeForce"   # so uma janela (Windows)
    python kasumi_bridge.py --help
"""
import argparse
import io
import socket
import struct
import sys
import threading
import time

try:
    from PIL import Image
except ImportError:
    sys.exit("Falta o Pillow:  pip install pillow")

PORT = 39770
VERSION = 1

T_DISCOVER, T_ANNOUNCE = 0x01, 0x02
T_HELLO, T_INPUT, T_STATS, T_TEXT, T_BYE = 0x10, 0x11, 0x12, 0x13, 0x14
T_VIDEO, T_AUDIO, T_WELCOME = 0x20, 0x21, 0x22

VIDEO_CHUNK = 1100
VIDEO_MAX = 64 * 1024
SCREEN_W, SCREEN_H = 400, 240
AUDIO_RATE = 24000
AUDIO_BLOCK = 1024

B_A, B_B, B_X, B_Y = 1 << 0, 1 << 1, 1 << 2, 1 << 3
B_LB, B_RB, B_BACK, B_START, B_GUIDE = 1 << 4, 1 << 5, 1 << 6, 1 << 7, 1 << 8
B_LS, B_RS = 1 << 9, 1 << 10
B_UP, B_DOWN, B_LEFT, B_RIGHT = 1 << 11, 1 << 12, 1 << 13, 1 << 14
M_LEFT, M_RIGHT = 0x01, 0x02
TEXT_F_ENTER = 0x01

CLIENT_TIMEOUT = 5.0


def header(t):
    return bytes((ord("K"), ord("O"), t, VERSION))


# ============================================================== IMA ADPCM

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
        50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
        253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
        1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
        3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
        11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
        32767]
INDEX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


class AdpcmEncoder:
    """IMA ADPCM mono, nibble baixo primeiro (igual ao decodificador do 3DS)."""

    def __init__(self):
        self.pred = 0
        self.idx = 0

    def encode(self, samples):
        """samples: lista de int16. Devolve (pred_inicial, idx_inicial, bytes)."""
        pred0, idx0 = self.pred, self.idx
        pred, idx = self.pred, self.idx
        out = bytearray((len(samples) + 1) // 2)
        for i, s in enumerate(samples):
            step = STEP[idx]
            diff = s - pred
            code = 0
            if diff < 0:
                code = 8
                diff = -diff
            delta = step >> 3
            if diff >= step:
                code |= 4
                diff -= step
                delta += step
            if diff >= step >> 1:
                code |= 2
                diff -= step >> 1
                delta += step >> 1
            if diff >= step >> 2:
                code |= 1
                delta += step >> 2
            pred = pred - delta if code & 8 else pred + delta
            if pred > 32767:
                pred = 32767
            elif pred < -32768:
                pred = -32768
            idx += INDEX[code]
            if idx < 0:
                idx = 0
            elif idx > 88:
                idx = 88
            if i & 1:
                out[i >> 1] |= code << 4
            else:
                out[i >> 1] = code
        self.pred, self.idx = pred, idx
        return pred0, idx0, bytes(out)


def audio_packet(seq, samples, enc):
    pred, idx, data = enc.encode(samples)
    return header(T_AUDIO) + struct.pack("<HHhBB", seq & 0xFFFF, len(samples), pred, idx, 0) + data


# ============================================================== video

def video_packets(frame_id, jpeg):
    """Fragmenta um JPEG em pacotes VIDEO (todos com 1100 bytes, menos o ultimo)."""
    total = len(jpeg)
    count = (total + VIDEO_CHUNK - 1) // VIDEO_CHUNK
    if count > 255 or total > VIDEO_MAX:
        raise ValueError("quadro grande demais")
    pkts = []
    for i in range(count):
        part = jpeg[i * VIDEO_CHUNK:(i + 1) * VIDEO_CHUNK]
        pkts.append(header(T_VIDEO) + struct.pack("<HBBI", frame_id & 0xFFFF, i, count, total) + part)
    return pkts


def fit_frame(img, stretch=False):
    """Encaixa a imagem em 400x240 (com tarjas pretas, ou esticada)."""
    if stretch:
        return img.resize((SCREEN_W, SCREEN_H), Image.BILINEAR)
    w, h = img.size
    scale = min(SCREEN_W / w, SCREEN_H / h)
    nw, nh = max(1, int(round(w * scale))), max(1, int(round(h * scale)))
    # reducao inteira primeiro: bem mais barato que redimensionar 1080p direto
    factor = int(1 / scale) // 2
    if factor >= 2:
        img = img.reduce(factor)
    img = img.resize((nw, nh), Image.BILINEAR)
    if (nw, nh) == (SCREEN_W, SCREEN_H):
        return img
    canvas = Image.new("RGB", (SCREEN_W, SCREEN_H))
    canvas.paste(img, ((SCREEN_W - nw) // 2, (SCREEN_H - nh) // 2))
    return canvas


def to_3ds_layout(img):
    """
    Gira 90 graus para o layout do framebuffer do 3DS: o JPEG resultante tem
    240x400 e cada linha dele e uma coluna da tela, de baixo para cima.
    Assim o 3DS decodifica direto no framebuffer, sem copiar nem girar.
    """
    return img.transpose(Image.Transpose.ROTATE_270)


def encode_jpeg(img, quality):
    for q in (quality, max(20, quality - 15), 20):
        buf = io.BytesIO()
        img.save(buf, "JPEG", quality=q, subsampling=2, optimize=False, progressive=False)
        data = buf.getvalue()
        if len(data) <= VIDEO_MAX:
            return data
    return None


# ============================================================== captura

class Capture:
    def __init__(self, monitor=1, window=None, region=None):
        try:
            import mss
        except ImportError:
            sys.exit("Falta o mss:  pip install mss")
        self.sct = None
        self.mss = mss
        self.monitor = monitor
        self.window = window
        self.region = region
        self._win_box = None
        self._win_check = 0.0

    def _box(self):
        if self.region:
            x, y, w, h = self.region
            return {"left": x, "top": y, "width": w, "height": h}
        if self.window:
            now = time.time()
            if now - self._win_check > 1.0:
                self._win_check = now
                self._win_box = self._find_window()
            if self._win_box:
                return self._win_box
        mons = self.sct.monitors
        return mons[self.monitor if self.monitor < len(mons) else 1]

    def _find_window(self):
        try:
            import pygetwindow as gw
        except ImportError:
            print("[aviso] --window precisa do pygetwindow (Windows). Usando a tela inteira.")
            self.window = None
            return None
        wins = [w for w in gw.getWindowsWithTitle(self.window) if w.width > 50 and w.height > 50]
        if not wins:
            return None
        w = wins[0]
        if getattr(w, "isMinimized", False):
            return None
        return {"left": w.left, "top": w.top, "width": w.width, "height": w.height}

    def grab(self):
        if self.sct is None:      # mss precisa ser criado na thread que o usa
            self.sct = self.mss.mss()
        shot = self.sct.grab(self._box())
        return Image.frombytes("RGB", shot.size, shot.bgra, "raw", "BGRX")


# ============================================================== controles

class PadOutput:
    """Controle de Xbox 360 virtual (vgamepad/ViGEm)."""

    def __init__(self):
        import vgamepad as vg
        self.vg = vg
        self.pad = vg.VX360Gamepad()
        X = vg.XUSB_BUTTON
        self.map = [
            (B_A, X.XUSB_GAMEPAD_A), (B_B, X.XUSB_GAMEPAD_B), (B_X, X.XUSB_GAMEPAD_X), (B_Y, X.XUSB_GAMEPAD_Y),
            (B_LB, X.XUSB_GAMEPAD_LEFT_SHOULDER), (B_RB, X.XUSB_GAMEPAD_RIGHT_SHOULDER),
            (B_BACK, X.XUSB_GAMEPAD_BACK), (B_START, X.XUSB_GAMEPAD_START), (B_GUIDE, X.XUSB_GAMEPAD_GUIDE),
            (B_LS, X.XUSB_GAMEPAD_LEFT_THUMB), (B_RS, X.XUSB_GAMEPAD_RIGHT_THUMB),
            (B_UP, X.XUSB_GAMEPAD_DPAD_UP), (B_DOWN, X.XUSB_GAMEPAD_DPAD_DOWN),
            (B_LEFT, X.XUSB_GAMEPAD_DPAD_LEFT), (B_RIGHT, X.XUSB_GAMEPAD_DPAD_RIGHT),
        ]
        self.last = None

    def apply(self, st):
        key = (st["buttons"], st["lx"], st["ly"], st["rx"], st["ry"], st["lt"], st["rt"])
        if key == self.last:
            return
        self.last = key
        for bit, btn in self.map:
            if st["buttons"] & bit:
                self.pad.press_button(button=btn)
            else:
                self.pad.release_button(button=btn)
        self.pad.left_joystick(x_value=st["lx"], y_value=st["ly"])
        self.pad.right_joystick(x_value=st["rx"], y_value=st["ry"])
        self.pad.left_trigger(value=st["lt"])
        self.pad.right_trigger(value=st["rt"])
        self.pad.update()

    def neutral(self):
        self.last = None
        self.pad.reset()
        self.pad.update()


class KeyboardOutput:
    """
    Plano B sem controle virtual: botoes viram teclas, analogico esquerdo vira
    WASD, analogico direito move o mouse, LT/RT viram botoes do mouse.
    Edite KEYMAP a vontade (bom para emuladores e jogos de PC sem controle).
    """

    def __init__(self, mouse, keyboard):
        from pynput.keyboard import Key
        self.kb = keyboard
        self.mouse = mouse
        self.Key = Key
        K = Key
        self.KEYMAP = [
            (B_A, K.space), (B_B, "e"), (B_X, "r"), (B_Y, "f"),
            (B_LB, "q"), (B_RB, "g"), (B_BACK, K.tab), (B_START, K.esc), (B_GUIDE, K.enter),
            (B_LS, K.shift), (B_RS, K.ctrl), (B_UP, K.up), (B_DOWN, K.down), (B_LEFT, K.left), (B_RIGHT, K.right),
        ]
        self.down = set()
        self.mbtn = set()

    def _set(self, key, on):
        if on and key not in self.down:
            self.kb.press(key)
            self.down.add(key)
        elif not on and key in self.down:
            self.kb.release(key)
            self.down.discard(key)

    def apply(self, st):
        b = st["buttons"]
        for bit, key in self.KEYMAP:
            self._set(key, bool(b & bit))
        t = 13000
        self._set("w", st["ly"] > t)
        self._set("s", st["ly"] < -t)
        self._set("a", st["lx"] < -t)
        self._set("d", st["lx"] > t)
        dx, dy = st["rx"] / 32767.0, -st["ry"] / 32767.0
        if abs(dx) > 0.05 or abs(dy) > 0.05:
            self.mouse.move(int(dx * abs(dx) * 18), int(dy * abs(dy) * 18))
        from pynput.mouse import Button
        for name, val, btn in (("lt", st["lt"], Button.right), ("rt", st["rt"], Button.left)):
            if val > 100 and name not in self.mbtn:
                self.mouse.press(btn)
                self.mbtn.add(name)
            elif val <= 100 and name in self.mbtn:
                self.mouse.release(btn)
                self.mbtn.discard(name)

    def neutral(self):
        for k in list(self.down):
            self._set(k, False)
        from pynput.mouse import Button
        for name in list(self.mbtn):
            self.mouse.release(Button.right if name == "lt" else Button.left)
        self.mbtn.clear()


def parse_input(p):
    seq, buttons, lx, ly, rx, ry, lt, rt, mdx, mdy, mbtn, flags = struct.unpack_from("<IIhhhhBBhhBB", p, 0)
    clamp = lambda v: max(-32767, min(32767, v))
    return {"seq": seq, "buttons": buttons, "lx": clamp(lx), "ly": clamp(ly), "rx": clamp(rx), "ry": clamp(ry),
            "lt": lt, "rt": rt, "mdx": mdx, "mdy": mdy, "mbtn": mbtn}


# ============================================================== ajuste automatico

class Adapter:
    """Ajusta qualidade/FPS pelas estatisticas que o 3DS manda a cada segundo."""

    def __init__(self):
        self.reset(15, 55, True)

    def reset(self, fps, quality, auto):
        self.max_fps, self.max_q = fps, quality
        self.fps, self.quality = fps, quality
        self.auto = auto
        self.good = 0

    def on_stats(self, ok, lost, dec_ms):
        if not self.auto:
            return
        total = ok + lost
        loss = lost / total if total else 0.0
        budget = 1000.0 / max(self.fps, 1)
        if dec_ms and dec_ms > budget * 0.85:
            # o 3DS nao da conta de decodificar: menos quadros (nao adianta mandar mais)
            self.fps = max(5, min(self.fps - 1, int(850 / dec_ms)))
            self.good = 0
        elif loss > 0.10:
            self.good = 0
            if self.quality > 30:
                self.quality = max(30, self.quality - 8)
            else:
                self.fps = max(5, self.fps - 2)
        elif loss < 0.02:
            self.good += 1
            if self.good >= 3:
                self.good = 0
                if self.fps < self.max_fps and (not dec_ms or dec_ms < budget * 0.6):
                    self.fps += 1
                elif self.quality < self.max_q:
                    self.quality = min(self.max_q, self.quality + 3)


# ============================================================== servidor

class Bridge:
    def __init__(self, args):
        self.args = args
        self.name = (args.name or socket.gethostname())[:31]
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1 << 20)
        except OSError:
            pass
        self.sock.bind(("0.0.0.0", args.port))
        self.lock = threading.Lock()
        self.client = None          # (ip, porta)
        self.last_seen = 0.0
        self.want_audio = False
        self.adapter = Adapter()
        self.running = True
        self.text_ids = []
        self.stats = {"frames": 0, "bytes": 0, "loss": 0, "dec": 0}

        self.output = None
        self.mouse = self.keyboard = None
        try:
            from pynput.mouse import Controller as MouseC
            from pynput.keyboard import Controller as KeyC
            self.mouse, self.keyboard = MouseC(), KeyC()
        except Exception:
            print("[aviso] pynput ausente: sem teclado remoto e sem mouse pelo toque.")
        if not args.keyboard:
            try:
                self.output = PadOutput()
                print("Controle virtual de Xbox 360 pronto (ViGEm).")
            except Exception as e:
                print(f"[aviso] controle virtual indisponivel ({e.__class__.__name__}).")
                print("        No Windows instale o ViGEmBus: https://github.com/nefarius/ViGEmBus/releases")
        if self.output is None and self.keyboard is not None:
            self.output = KeyboardOutput(self.mouse, self.keyboard)
            print("Usando teclado e mouse no lugar do controle (veja KEYMAP no codigo).")

    # ---------------- rede
    def send(self, data, addr=None):
        addr = addr or self.client
        if addr:
            try:
                self.sock.sendto(data, addr)
            except OSError:
                pass

    def active(self):
        with self.lock:
            return self.client is not None and time.time() - self.last_seen < CLIENT_TIMEOUT

    def recv_loop(self):
        prev_mbtn = 0
        while self.running:
            try:
                data, addr = self.sock.recvfrom(2048)
            except OSError:
                continue
            if len(data) < 4 or data[0] != ord("K") or data[1] != ord("O") or data[3] != VERSION:
                continue
            t, p = data[2], data[4:]

            if t == T_DISCOVER:
                self.send(header(T_ANNOUNCE) + self.name.encode("utf-8")[:31], addr)
                continue

            if t == T_HELLO and len(p) >= 4:
                fps, q, audio, flags = p[0], p[1], p[2], p[3]
                fps = max(5, min(30, fps))
                q = max(20, min(90, q))
                with self.lock:
                    new = self.client != addr or time.time() - self.last_seen > CLIENT_TIMEOUT
                    self.client = addr
                    self.last_seen = time.time()
                    self.want_audio = bool(audio) and not self.args.no_audio
                    self.adapter.reset(fps, q, bool(flags & 1))
                self.send(header(T_WELCOME) + self.name.encode("utf-8")[:31], addr)
                if new:
                    print(f"\n>> 3DS conectado: {addr[0]}  ({fps} fps, qualidade {q}, audio {'sim' if audio else 'nao'})")
                continue

            with self.lock:
                if addr != self.client:
                    continue
                self.last_seen = time.time()

            if t == T_INPUT and len(p) >= 24:
                st = parse_input(p)
                if self.output:
                    self.output.apply(st)
                if self.mouse is not None:
                    if st["mdx"] or st["mdy"]:
                        self.mouse.move(st["mdx"], st["mdy"])
                    self._mouse_buttons(prev_mbtn, st["mbtn"])
                    prev_mbtn = st["mbtn"]
            elif t == T_STATS and len(p) >= 8:
                ok, lost, dec, alost = struct.unpack_from("<HHHH", p, 0)
                with self.lock:
                    self.adapter.on_stats(ok, lost, dec)
                total = ok + lost
                self.stats["loss"] = int(100 * lost / total) if total else 0
                self.stats["dec"] = dec
            elif t == T_TEXT and len(p) >= 3:
                tid, flags = struct.unpack_from("<HB", p, 0)
                if tid in self.text_ids:
                    continue
                self.text_ids = (self.text_ids + [tid])[-16:]
                text = p[3:].decode("utf-8", "replace")
                if self.keyboard is not None:
                    try:
                        if text:
                            self.keyboard.type(text)
                        if flags & TEXT_F_ENTER:
                            from pynput.keyboard import Key
                            self.keyboard.press(Key.enter)
                            self.keyboard.release(Key.enter)
                    except Exception as e:
                        print(f"[aviso] nao consegui digitar: {e}")
            elif t == T_BYE:
                print("\n<< 3DS desconectou.")
                with self.lock:
                    self.client = None
                if self.output:
                    self.output.neutral()

    def _mouse_buttons(self, prev, cur):
        from pynput.mouse import Button
        for bit, btn in ((M_LEFT, Button.left), (M_RIGHT, Button.right)):
            if cur & bit and not prev & bit:
                self.mouse.press(btn)
            elif prev & bit and not cur & bit:
                self.mouse.release(btn)

    # ---------------- video
    def video_loop(self):
        cap = Capture(self.args.monitor, self.args.window, self.args.region)
        frame_id = 0
        was_active = False
        while self.running:
            if not self.active():
                if was_active:
                    print("\n.. sem sinal do 3DS (tampa fechada?). Aguardando...")
                    if self.output:
                        self.output.neutral()
                was_active = False
                time.sleep(0.1)
                continue
            was_active = True
            with self.lock:
                fps, q = self.adapter.fps, self.adapter.quality
            t0 = time.perf_counter()
            try:
                img = fit_frame(cap.grab(), self.args.stretch)
                jpeg = encode_jpeg(to_3ds_layout(img), q)
            except Exception as e:
                print(f"[aviso] captura falhou: {e}")
                time.sleep(0.5)
                continue
            if jpeg:
                pkts = video_packets(frame_id, jpeg)
                frame_id = (frame_id + 1) & 0xFFFF
                # espalha os fragmentos: o Wi-Fi do 3DS perde rajadas grandes
                gap = min(0.0012, 0.35 / fps / max(1, len(pkts)))
                nxt = time.perf_counter()
                for pk in pkts:
                    while time.perf_counter() < nxt:
                        pass
                    self.send(pk)
                    nxt += gap
                self.stats["frames"] += 1
                self.stats["bytes"] += sum(len(x) for x in pkts)
            spare = 1.0 / fps - (time.perf_counter() - t0)
            if spare > 0:
                time.sleep(spare)

    # ---------------- audio
    def audio_loop(self):
        if self.args.no_audio:
            return
        try:
            import numpy as np
            import soundcard as sc
        except ImportError:
            print("[aviso] sem audio: pip install soundcard numpy")
            return
        try:
            mic = sc.get_microphone(id=str(sc.default_speaker().name), include_loopback=True)
        except Exception as e:
            print(f"[aviso] sem audio: nao achei a saida de som para capturar ({e})")
            return
        enc = AdpcmEncoder()
        seq = 0
        with mic.recorder(samplerate=48000, channels=2, blocksize=1024) as rec:
            while self.running:
                data = rec.record(numframes=AUDIO_BLOCK * 2)   # 48 kHz estereo
                if not (self.active() and self.want_audio):
                    continue
                mono = data.mean(axis=1) if data.ndim == 2 else data
                mono = mono[: AUDIO_BLOCK * 2].reshape(-1, 2).mean(axis=1)  # 48k -> 24k
                pcm = np.clip(mono * 32767.0, -32768, 32767).astype(np.int16).tolist()
                if len(pcm) < AUDIO_BLOCK:
                    pcm += [0] * (AUDIO_BLOCK - len(pcm))
                self.send(audio_packet(seq, pcm, enc))
                seq = (seq + 1) & 0xFFFF

    # ---------------- painel
    def status_loop(self):
        while self.running:
            time.sleep(2.0)
            if not self.active():
                continue
            fr, by = self.stats["frames"], self.stats["bytes"]
            self.stats["frames"] = self.stats["bytes"] = 0
            with self.lock:
                q, fps = self.adapter.quality, self.adapter.fps
            line = (f"\r   {fr / 2:4.1f} fps enviados | {by * 8 / 2000:5.0f} kbps | alvo {fps} fps q{q} | "
                    f"perda {self.stats['loss']}% | 3DS decodifica em {self.stats['dec']} ms   ")
            sys.stdout.write(line)
            sys.stdout.flush()

    def run(self):
        ips = local_ips()
        print("=" * 60)
        print(f" Kasumi Bridge  |  nome: {self.name}  |  porta UDP {self.args.port}")
        print(f" IP deste PC: {', '.join(ips) if ips else '(desconhecido)'}")
        print(" No 3DS: abra o Kasumi Old; este PC aparece na lista.")
        print(" Se nao aparecer, aperte X no 3DS e digite o IP acima.")
        print("=" * 60)
        for target in (self.recv_loop, self.video_loop, self.audio_loop, self.status_loop):
            threading.Thread(target=target, daemon=True).start()
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("\nEncerrando.")
            self.running = False
            if self.output:
                self.output.neutral()


def local_ips():
    ips = set()
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("10.255.255.255", 1))
        ips.add(s.getsockname()[0])
        s.close()
    except OSError:
        pass
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            ip = info[4][0]
            if not ip.startswith("127."):
                ips.add(ip)
    except OSError:
        pass
    return sorted(ips)


def main():
    ap = argparse.ArgumentParser(description="Kasumi Bridge: manda a tela do PC para o Kasumi Old no 3DS.")
    ap.add_argument("--name", help="nome que aparece no 3DS (padrao: nome do PC)")
    ap.add_argument("--port", type=int, default=PORT)
    ap.add_argument("--monitor", type=int, default=1, help="monitor a capturar (1 = principal)")
    ap.add_argument("--window", help="capturar so a janela cujo titulo contem este texto (Windows)")
    ap.add_argument("--region", help="capturar a regiao x,y,largura,altura")
    ap.add_argument("--stretch", action="store_true", help="esticar para 400x240 em vez de tarjas pretas")
    ap.add_argument("--no-audio", action="store_true", help="nao mandar som")
    ap.add_argument("--keyboard", action="store_true", help="usar teclado/mouse em vez do controle virtual")
    args = ap.parse_args()
    if args.region:
        try:
            args.region = tuple(int(v) for v in args.region.split(","))
            assert len(args.region) == 4
        except Exception:
            sys.exit("--region deve ser x,y,largura,altura (ex.: 0,0,1280,720)")
    Bridge(args).run()


if __name__ == "__main__":
    main()
