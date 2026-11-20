#!/usr/bin/env python3
"""IPC preload decodes into the cache. Usage: check_preload.py build/gslapper"""
import os
from pathlib import Path
import sys
import time

from headless import BLUE, RED, HeadlessSway, capture, ipc, is_close, solid_png, wait_for_socket

BINARY = str(Path(sys.argv[1]).resolve())


def start(sway, image, *options):
    sock = sway.root / "ipc.sock"
    player = sway.spawn([BINARY, "--no-save-state", "-I", str(sock), *options, "*", str(image)],
                        name="gslapper")
    wait_for_socket(sock, process=player)
    return sock


with HeadlessSway() as sway:
    red, blue = sway.root / "red.png", sway.root / "blue.png"
    solid_png(red, RED)
    solid_png(blue, BLUE, 640, 360)
    sock = start(sway, red)

    assert ipc(sock, f"preload {blue}") == "OK: preloaded\n", ipc(sock, "cache-list")
    assert os.path.realpath(blue) in ipc(sock, "cache-list"), ipc(sock, "cache-list")
    assert ipc(sock, f"preload {blue}") == "OK: already cached\n"
    assert is_close(capture(sway.env).pixel(320, 180), RED), "preload changed the screen"

    started = time.monotonic()
    assert ipc(sock, f"change {blue}").startswith("OK")
    while not is_close(capture(sway.env).pixel(320, 180), BLUE):
        assert time.monotonic() - started < 0.5, "cached change was slow to appear"

    reply = ipc(sock, "preload /nonexistent.png")
    assert reply == "ERROR: file not accessible\n", reply

with HeadlessSway() as sway:
    red, blue = sway.root / "red.png", sway.root / "blue.png"
    solid_png(red, RED)
    solid_png(blue, BLUE)
    sock = start(sway, red, "--cache-size", "0")
    reply = ipc(sock, f"preload {blue}")
    assert reply == "ERROR: cache disabled\n", reply

with HeadlessSway() as sway:
    red, big = sway.root / "red.png", sway.root / "big.png"
    solid_png(red, RED)
    solid_png(big, BLUE, 1000, 1000)
    sock = start(sway, red, "--cache-size", "1")
    reply = ipc(sock, f"preload {big}")
    assert reply == "ERROR: image exceeds cache size\n", reply

print("Preload decodes into the cache")
