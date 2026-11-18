#!/usr/bin/env python3
"""Run as an ordinary user with Sway, software Mesa and a wallpaper image."""
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

binary, wallpaper = (str(Path(arg).resolve()) for arg in sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="gslapper-") as directory:
    root = Path(directory)
    config = root / "sway.conf"
    config.write_text("output HEADLESS-1 resolution 320x240\nxwayland disable\n")
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS="headless",
               WLR_RENDERER="pixman", LIBGL_ALWAYS_SOFTWARE="1")
    processes = []
    try:
        with (root / "sway.log").open("w+") as log:
            compositor = subprocess.Popen(
                ["sway", "--unsupported-gpu", "-c", str(config)], env=env,
                stdin=subprocess.DEVNULL, stdout=log, stderr=log)
            processes.append(compositor)
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                displays = [p for p in root.glob("wayland-*") if p.is_socket()]
                if displays:
                    break
                if compositor.poll() is not None:
                    log.seek(0)
                    raise RuntimeError(log.read())
                time.sleep(0.1)
            else:
                raise AssertionError("headless compositor did not start")
            env["WAYLAND_DISPLAY"] = displays[0].name
            ipc = root / "ipc.sock"
            player = subprocess.Popen(
                [binary, "--no-save-state", "-I", str(ipc), "*", wallpaper],
                env=env, stdin=subprocess.DEVNULL, stdout=log, stderr=log)
            processes.append(player)
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                try:
                    with socket.socket(socket.AF_UNIX) as client:
                        client.settimeout(0.5)
                        client.connect(str(ipc))
                        client.sendall(b"query\n")
                        if b"STATUS:" in client.recv(4096):
                            break
                except OSError:
                    pass
                time.sleep(0.1)
            else:
                log.seek(0)
                raise RuntimeError("IPC did not start:\n" + log.read())
            compositor.terminate()
            compositor.wait(timeout=10)
            status = player.wait(timeout=10)
            assert status == 0, f"gSlapper exited with status {status}"
            assert not ipc.exists(), "IPC socket remains after compositor disconnect"
            print("Compositor disconnect removes the wallpaper IPC socket")
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
