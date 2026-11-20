#!/usr/bin/env python3
"""Slideshow (-n) checks. Usage: check_slideshow.py build/gslapper [check...]"""
from pathlib import Path
import subprocess
import sys
import time

from headless import (BLUE, GREEN, RED, HeadlessSway, capture, ipc, is_close, solid_png,
                      wait_for_socket)

BINARY = str(Path(sys.argv[1]).resolve())


def slides(sway):
    folder = sway.root / "slides"
    folder.mkdir()
    for name, rgb in (("1-red.png", RED), ("2-green.png", GREEN), ("3-blue.png", BLUE)):
        solid_png(folder / name, rgb)
    return folder


def start(sway, source, *options):
    sock = sway.root / "ipc.sock"
    player = sway.spawn(["stdbuf", "-oL", "-eL", BINARY, "--no-save-state", "-I", str(sock),
                         *options, "*", str(source)], name="gslapper")
    wait_for_socket(sock, process=player)
    return sock, player, time.monotonic()


def at(t0, seconds):
    delay = t0 + seconds - time.monotonic()
    if delay > 0:
        time.sleep(delay)


def centre(sway):
    return capture(sway.env).pixel(320, 180)


def expect_centre(sway, rgb, label):
    actual = centre(sway)
    assert is_close(actual, rgb), f"{label}: centre is {actual}, expected {rgb}"


def check_slideshow_cycle():
    with HeadlessSway() as sway:
        sock, _, t0 = start(sway, slides(sway), "-n", "2", "--transition-type", "none")
        for seconds, rgb in ((1, RED), (3, GREEN), (5, BLUE), (7, RED)):
            at(t0, seconds)
            expect_centre(sway, rgb, f"t={seconds}s")


def check_slideshow_pause():
    with HeadlessSway() as sway:
        sock, _, t0 = start(sway, slides(sway), "-n", "2", "--transition-type", "none")
        at(t0, 1)
        assert ipc(sock, "pause").startswith("OK")
        at(t0, 5)
        expect_centre(sway, RED, "while paused")
        assert ipc(sock, "resume").startswith("OK")
        resumed = time.monotonic()
        at(resumed, 1)
        expect_centre(sway, RED, "1s after resume")
        at(resumed, 2.7)
        expect_centre(sway, GREEN, "after a full interval")


def check_slideshow_pause_after_wrap():
    with HeadlessSway() as sway:
        sock, _, t0 = start(sway, slides(sway), "-n", "1", "--transition-type", "none")
        at(t0, 3.5)  # red again, this time from the cache
        expect_centre(sway, RED, "wrapped to the first image")
        assert ipc(sock, "pause") == "OK\n"
        at(t0, 6.5)
        expect_centre(sway, RED, "paused after wrap")


def check_slideshow_two_outputs():
    with HeadlessSway(outputs=2) as sway:
        sock, _, t0 = start(sway, slides(sway), "-n", "2", "--transition-type", "none")
        at(t0, 3)
        frame = capture(sway.env)
        for x in (320, 960):
            assert is_close(frame.pixel(x, 180), GREEN), f"x={x}: {frame.pixel(x, 180)}"


def check_slideshow_auto_pause():
    with HeadlessSway() as sway:
        sock, _, t0 = start(sway, slides(sway), "-p", "-n", "2", "--transition-type", "none")
        for seconds, rgb in ((5, BLUE), (7, RED), (9, GREEN)):
            at(t0, seconds)
            expect_centre(sway, rgb, f"t={seconds}s with --auto-pause")


def check_slideshow_uncached():
    # Every advance decodes through a fresh pipeline
    with HeadlessSway() as sway:
        sock, player, t0 = start(sway, slides(sway), "-n", "1", "--cache-size", "0",
                                 "--transition-type", "none")
        at(t0, 12)
        assert player.poll() is None, "gslapper exited during uncached slideshow"
        assert "CRITICAL" not in sway.log("gslapper"), sway.log("gslapper")[-1500:]
        assert ipc(sock, "query").startswith("STATUS: playing image")


def check_slideshow_ipc_change():
    with HeadlessSway() as sway:
        folder = slides(sway)
        sock, _, t0 = start(sway, folder, "-n", "2", "--transition-type", "none")
        at(t0, 1)
        assert ipc(sock, f"change {folder / '3-blue.png'}").startswith("OK")
        at(t0, 2.5)
        expect_centre(sway, BLUE, "timer restarted by change")
        at(t0, 4)
        expect_centre(sway, GREEN, "list continues after red")


def check_slideshow_rejects_video():
    with HeadlessSway() as sway:
        folder = slides(sway)
        (folder / "clip.mp4").write_bytes(b"")
        playlist = folder / "list.m3u"
        playlist.write_text("1-red.png\nclip.mp4\n")
        player = sway.spawn(["stdbuf", "-oL", "-eL", BINARY, "--no-save-state", "-n", "2",
                             "*", str(playlist)], name="gslapper")
        try:
            status = player.wait(timeout=5)
        except subprocess.TimeoutExpired:
            raise AssertionError("gslapper kept running with a video in the playlist")
        assert status != 0, "exit status 0"
        log = sway.log("gslapper")
        assert "slideshow supports static images only" in log, log[-1500:]


def check_slideshow_with_transition():
    with HeadlessSway() as sway:
        sock, _, t0 = start(sway, slides(sway), "-n", "2", "--transition-type", "left",
                            "--transition-duration", "1")
        at(t0, 2.35)
        frame = capture(sway.env)
        assert is_close(frame.pixel(40, 180), GREEN), f"left edge {frame.pixel(40, 180)}"
        assert is_close(frame.pixel(600, 180), RED), f"right edge {frame.pixel(600, 180)}"


def check_slideshow_negative_interval():
    # A negative -n used to wrap the unsigned interval and silently never advance
    with HeadlessSway() as sway:
        image = sway.root / "red.png"
        solid_png(image, RED)
        start(sway, image, "-n", "-5")
        time.sleep(0.5)
        log = sway.log("gslapper")
        assert "0 or invalid time set for slideshow" in log, log[-1500:]
        expect_centre(sway, RED, "image still shown")


CHECKS = [check_slideshow_cycle, check_slideshow_pause, check_slideshow_pause_after_wrap,
          check_slideshow_two_outputs, check_slideshow_auto_pause, check_slideshow_uncached,
          check_slideshow_ipc_change,
          check_slideshow_rejects_video, check_slideshow_with_transition,
          check_slideshow_negative_interval]

if __name__ == "__main__":
    selected = sys.argv[2:]
    failed = 0
    for check in CHECKS:
        if selected and check.__name__ not in selected:
            continue
        try:
            check()
            print(f"PASS {check.__name__}")
        except Exception as error:
            failed += 1
            print(f"FAIL {check.__name__}: {error}")
    sys.exit(1 if failed else 0)
