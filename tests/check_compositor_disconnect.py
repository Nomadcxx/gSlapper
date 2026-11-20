#!/usr/bin/env python3
"""Run as an ordinary user with Sway, software Mesa and a wallpaper image."""
from pathlib import Path
import sys
import time

from headless import HeadlessSway, ipc as ipc_request, wait_for_socket

binary, wallpaper = (str(Path(arg).resolve()) for arg in sys.argv[1:])
with HeadlessSway(320, 240) as sway:
    ipc = sway.root / "ipc.sock"
    player = sway.spawn([binary, "--no-save-state", "-I", str(ipc), "*", wallpaper],
                        name="gslapper")
    try:
        wait_for_socket(ipc, process=player)
        deadline = time.monotonic() + 15
        while "STATUS:" not in ipc_request(ipc, "query"):
            if time.monotonic() > deadline:
                raise AssertionError("query never answered")
            time.sleep(0.1)
    except Exception as error:
        raise RuntimeError(f"IPC did not start: {error}\n" + sway.log("gslapper"))
    compositor = sway.processes[0]
    compositor.terminate()
    compositor.wait(timeout=10)
    status = player.wait(timeout=10)
    assert status == 0, f"gSlapper exited with status {status}"
    assert not ipc.exists(), "IPC socket remains after compositor disconnect"
    print("Compositor disconnect removes the wallpaper IPC socket")
