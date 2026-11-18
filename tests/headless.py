"""Headless sway harness for gSlapper runtime checks.

Needs sway, grim and software Mesa. Run the check scripts as an ordinary user.
"""
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import time
import zlib


class HeadlessSway:
    def __init__(self, width=640, height=360):
        self.width = width
        self.height = height
        self.processes = []
        self.root = None
        self.env = None

    def __enter__(self):
        # Short base dir: socket paths must fit in sun_path (108 bytes)
        self.root = Path(tempfile.mkdtemp(prefix="gsl-", dir="/tmp"))
        config = self.root / "sway.conf"
        config.write_text(f"output HEADLESS-1 resolution {self.width}x{self.height}\n"
                          "xwayland disable\n")
        self.env = dict(os.environ, XDG_RUNTIME_DIR=str(self.root), WLR_BACKENDS="headless",
                        WLR_RENDERER="pixman", LIBGL_ALWAYS_SOFTWARE="1")
        self.env.pop("WAYLAND_DISPLAY", None)
        self.env.pop("DISPLAY", None)
        compositor = self.spawn(["sway", "--unsupported-gpu", "-c", str(config)], name="sway")
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            displays = [p for p in self.root.glob("wayland-*") if p.is_socket()]
            if displays:
                self.env["WAYLAND_DISPLAY"] = displays[0].name
                return self
            if compositor.poll() is not None:
                raise RuntimeError("sway exited:\n" + self.log("sway"))
            time.sleep(0.1)
        raise AssertionError("headless compositor did not start")

    def __exit__(self, *exc):
        for process in reversed(self.processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        shutil.rmtree(self.root, ignore_errors=True)
        return False

    def spawn(self, args, name=None):
        name = name or Path(args[0]).name
        log = (self.root / f"{name}.log").open("w")
        process = subprocess.Popen(args, env=self.env, stdin=subprocess.DEVNULL,
                                   stdout=log, stderr=subprocess.STDOUT)
        process.log_name = name
        self.processes.append(process)
        return process

    def log(self, name):
        path = self.root / f"{name}.log"
        return path.read_text(errors="replace") if path.exists() else ""


class Frame:
    def __init__(self, width, height, data):
        self.width = width
        self.height = height
        self.data = data

    def pixel(self, x, y):
        i = (y * self.width + x) * 3
        return tuple(self.data[i:i + 3])


def capture(env):
    ppm = subprocess.run(["grim", "-t", "ppm", "-"], env=env, check=True,
                         capture_output=True).stdout
    # P6 header: magic, width, height, maxval, then one whitespace byte
    fields = []
    pos = 0
    while len(fields) < 4:
        while ppm[pos:pos + 1].isspace():
            pos += 1
        if ppm[pos:pos + 1] == b"#":
            pos = ppm.index(b"\n", pos) + 1
            continue
        end = pos
        while not ppm[end:end + 1].isspace():
            end += 1
        fields.append(ppm[pos:end])
        pos = end
    assert fields[0] == b"P6" and fields[3] == b"255", fields
    return Frame(int(fields[1]), int(fields[2]), ppm[pos + 1:])


def write_png(path, width, height, pixel_fn):
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(pixel_fn(x, y))

    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))

    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" +
                           chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
                           chunk(b"IDAT", zlib.compress(bytes(rows))) +
                           chunk(b"IEND", b""))


def solid_png(path, rgb, width=64, height=64):
    write_png(path, width, height, lambda x, y: rgb)


def wait_for_socket(path, timeout=15, process=None):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if Path(path).is_socket():
            try:
                with socket.socket(socket.AF_UNIX) as s:
                    s.connect(str(path))
                return
            except OSError:
                pass
        if process is not None and process.poll() is not None:
            raise RuntimeError(f"process exited with status {process.returncode}")
        time.sleep(0.1)
    raise AssertionError(f"IPC socket {path} did not appear")


def ipc(path, line, timeout=10):
    with socket.socket(socket.AF_UNIX) as s:
        s.settimeout(timeout)
        s.connect(str(path))
        s.sendall(line.encode() + b"\n")
        # The server keeps the connection until the client half-closes, as nc does
        s.shutdown(socket.SHUT_WR)
        reply = b""
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            reply += chunk
    return reply.decode(errors="replace")


def is_close(rgb, expected, tol=40):
    return all(abs(a - b) <= tol for a, b in zip(rgb, expected))


RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
BLACK = (0, 0, 0)
