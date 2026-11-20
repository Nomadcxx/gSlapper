#!/usr/bin/env python3
"""IPC cache commands reach the image cache. Usage: check_ipc_cache.py build/gslapper"""
from pathlib import Path
import re
import sys

from headless import GREEN, RED, HeadlessSway, ipc, solid_png, wait_for_socket


def entries(sock):
    reply = ipc(sock, "cache-stats")
    match = re.search(r"\((\d+) images?\)", reply)
    assert match, f"unexpected cache-stats reply: {reply!r}"
    return int(match.group(1))


binary = str(Path(sys.argv[1]).resolve())
with HeadlessSway() as sway:
    red, green = sway.root / "red.png", sway.root / "green.png"
    solid_png(red, RED)
    solid_png(green, GREEN)
    sock = sway.root / "ipc.sock"
    player = sway.spawn([binary, "--no-save-state", "-I", str(sock), "*", str(red)],
                        name="gslapper")
    wait_for_socket(sock, process=player)
    assert ipc(sock, f"change {green}").startswith("OK"), "change failed"
    assert entries(sock) == 2, ipc(sock, "cache-stats")

    reply = ipc(sock, "unload unused")
    assert reply == "OK: unused entries cleared\n", reply
    assert entries(sock) == 1, ipc(sock, "cache-stats")

    reply = ipc(sock, "unload all")
    assert reply == "OK: cache cleared\n", reply

    reply = ipc(sock, "list")
    assert reply.startswith("ERROR: unknown command"), reply
    print("IPC cache commands reach the cache")
