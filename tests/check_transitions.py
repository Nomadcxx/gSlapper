#!/usr/bin/env python3
"""Transition rendering and IPC checks. Usage: check_transitions.py build/gslapper [check...]"""
from pathlib import Path
import shutil
import subprocess
import sys
import time

from headless import (BLACK, BLUE, GREEN, RED, HeadlessSway, capture, ipc, is_close,
                      solid_png, wait_for_socket, write_png)

BINARY = str(Path(sys.argv[1]).resolve())


def start(sway, image, *options):
    sock = sway.root / "ipc.sock"
    player = sway.spawn(["stdbuf", "-oL", "-eL", BINARY, "--no-save-state", "-I", str(sock),
                         *options, "*", str(image)], name="gslapper")
    wait_for_socket(sock, process=player)
    time.sleep(0.5)
    return sock, player


def images(sway):
    paths = {}
    for name, rgb in (("red", RED), ("green", GREEN), ("blue", BLUE)):
        paths[name] = sway.root / f"{name}.png"
        solid_png(paths[name], rgb)
    return paths


def change_and_capture(sway, sock, image, delay):
    reply = ipc(sock, f"change {image}")
    assert reply.startswith("OK"), reply
    time.sleep(delay)
    return capture(sway.env)


def expect(frame, point, rgb, label):
    actual = frame.pixel(*point)
    assert is_close(actual, rgb), f"{label}: pixel {point} is {actual}, expected {rgb}"


def check_instant_change():
    with HeadlessSway() as sway:
        img = images(sway)
        sock, _ = start(sway, img["red"])
        expect(change_and_capture(sway, sock, img["blue"], 0.5), (320, 180), BLUE, "decoded change")
        expect(change_and_capture(sway, sock, img["red"], 0.5), (320, 180), RED, "cached change")


def check_left_wipe():
    with HeadlessSway() as sway:
        img = images(sway)
        sock, _ = start(sway, img["red"], "--transition-type", "left",
                        "--transition-duration", "4")
        frame = change_and_capture(sway, sock, img["blue"], 1.0)
        expect(frame, (40, 180), BLUE, "left edge")
        expect(frame, (600, 180), RED, "right edge")


def check_center_round():
    with HeadlessSway(640, 480) as sway:
        img = images(sway)
        sock, _ = start(sway, img["red"], "--transition-type", "center",
                        "--transition-duration", "5")
        frame = change_and_capture(sway, sock, img["blue"], 2.5)
        expect(frame, (320, 240), BLUE, "centre")
        for corner in ((2, 2), (637, 2), (2, 477), (637, 477)):
            expect(frame, corner, RED, "corner")
        # Equal distances along x and y must agree, or the circle is an ellipse
        expect(frame, (460, 240), BLUE, "x inside")
        expect(frame, (320, 380), BLUE, "y inside")
        expect(frame, (85, 240), RED, "x outside")
        expect(frame, (320, 475), RED, "y outside")


def check_outer():
    with HeadlessSway() as sway:
        img = images(sway)
        sock, _ = start(sway, img["red"], "--transition-type", "outer",
                        "--transition-duration", "5")
        frame = change_and_capture(sway, sock, img["blue"], 2.5)
        expect(frame, (320, 180), RED, "centre")
        expect(frame, (2, 2), BLUE, "corner")
        expect(frame, (637, 357), BLUE, "corner")


def check_fade_midway():
    with HeadlessSway() as sway:
        img = images(sway)
        sock, _ = start(sway, img["red"], "--transition-type", "fade",
                        "--transition-duration", "4")
        frame = change_and_capture(sway, sock, img["blue"], 2.0)
        r, _, b = frame.pixel(320, 180)
        assert 60 <= r <= 200 and 60 <= b <= 200, f"fade midway pixel {frame.pixel(320, 180)}"


def banded_png(path):
    # 4:3 red image whose top quarter is green
    write_png(path, 400, 300, lambda x, y: GREEN if y < 75 else RED)


def check_old_image_aspect():
    with HeadlessSway() as sway:
        img = images(sway)
        banded = sway.root / "banded.png"
        banded_png(banded)
        sock, player = start(sway, banded, "--transition-type", "left",
                             "--transition-duration", "4")
        frame = change_and_capture(sway, sock, img["blue"], 1.0)
        # Fill crops 60px off the top, so the green band ends at y=60. Drawn on the
        # incoming square image's quad instead, it would end near y=20.
        expect(frame, (600, 40), GREEN, "fill crop of outgoing image")
        expect(frame, (600, 75), RED, "outgoing image body")

    with HeadlessSway() as sway:
        img = images(sway)
        banded = sway.root / "banded.png"
        banded_png(banded)
        sock, player = start(sway, banded, "-o", "panscan=1.0", "--transition-type", "left",
                             "--transition-duration", "4")
        frame = change_and_capture(sway, sock, img["blue"], 1.0)
        # Fit pillarboxes the 4:3 image to x 80..560 (the square would give 140..500)
        expect(frame, (620, 180), BLACK, "pillarbox of outgoing image")
        expect(frame, (530, 180), RED, "outgoing image body")


def check_no_black_flash():
    with HeadlessSway() as sway:
        img = images(sway)
        big = sway.root / "big.png"
        solid_png(big, BLUE, 6000, 4000)
        sock, _ = start(sway, img["red"], "--transition-type", "fade",
                        "--transition-duration", "0.6")
        ipc_reply = ipc(sock, f"change {big}")
        assert ipc_reply.startswith("OK"), ipc_reply
        samples = []
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            samples.append(capture(sway.env).pixel(320, 180))
        assert not any(is_close(s, BLACK) for s in samples), f"black frame in {samples}"
        assert is_close(samples[-1], BLUE), f"did not end on the new image: {samples[-1]}"
        # The fade clock starts once the new image is on screen, so the fade is visible
        mixed = [s for s in samples if not is_close(s, RED) and not is_close(s, BLUE)]
        assert mixed, f"no intermediate fade frames in {samples}"


def check_failed_change_keeps_image():
    for options in ((), ("--transition-type", "fade", "--transition-duration", "1")):
        with HeadlessSway() as sway:
            img = images(sway)
            broken = sway.root / "broken.png"
            broken.write_bytes(b"\x89PNG\r\n\x1a\n not really a png")
            sock, player = start(sway, img["red"], *options)
            reply = ipc(sock, f"change {broken}")
            if not options:
                assert reply == "ERROR: failed to load image\n", reply
            time.sleep(2)
            assert player.poll() is None, f"gslapper exited ({options or 'no transition'})"
            expect(capture(sway.env), (320, 180), RED, "image after failed change")
            # Still usable afterwards
            expect(change_and_capture(sway, sock, img["blue"], 2.0), (320, 180), BLUE,
                   "change after failure")


def check_rapid_changes():
    with HeadlessSway() as sway:
        img = images(sway)
        sock, _ = start(sway, img["red"], "--transition-type", "wipe",
                        "--transition-duration", "1")
        for name in ("green", "blue", "red", "blue"):
            assert ipc(sock, f"change {img[name]}").startswith("OK")
        time.sleep(5)
        expect(capture(sway.env), (320, 180), BLUE, "final image")
        log = sway.log("gslapper")
        assert "OpenGL error" not in log, log[-2000:]


def check_video_change_ignores_transition():
    if not shutil.which("gst-launch-1.0") or subprocess.run(
            ["gst-inspect-1.0", "x264enc"], capture_output=True).returncode != 0:
        print("SKIP check_video_change_ignores_transition: x264enc not available")
        return
    with HeadlessSway() as sway:
        img = images(sway)
        video = sway.root / "clip.mp4"
        subprocess.run(["gst-launch-1.0", "-q", "videotestsrc", "num-buffers=60",
                        "pattern=ball", "!", "video/x-raw,width=320,height=180", "!",
                        "x264enc", "!", "mp4mux", "!", "filesink", f"location={video}"],
                       check=True)
        sock, player = start(sway, video, "-o", "loop")
        assert ipc(sock, "set-transition wipe").startswith("OK")
        reply = ipc(sock, f"change {img['red']}")
        assert reply == "ERROR: cannot update path (use --auto-stop for video changes)\n", reply
        time.sleep(1)
        assert player.poll() is None, "gslapper exited"
        assert not is_close(capture(sway.env).pixel(320, 180), RED), "video replaced by image"


def check_ipc_contract():
    with HeadlessSway() as sway:
        img = images(sway)
        sock, _ = start(sway, img["red"])
        reply = ipc(sock, "get-transition")
        assert reply == "TRANSITION: none disabled 0.50 angle=45 pos=0.50,0.50\n", repr(reply)
        assert ipc(sock, "set-transition fade") == "OK: fade transitions enabled\n"
        assert ipc(sock, "set-transition none") == "OK: transitions disabled\n"
        assert ipc(sock, "set-transition wave") == "OK: wave transitions enabled\n"
        assert ipc(sock, "set-transition sparkle") == "ERROR: unknown transition type\n"
        assert ipc(sock, "set-transition-angle 120") == "OK: angle set to 120\n"
        assert ipc(sock, "set-transition-angle x") == "ERROR: invalid angle\n"
        assert ipc(sock, "set-transition-pos top-left") == "OK: position set to 0.00,0.00\n"
        assert ipc(sock, "set-transition-pos 2,2") == "ERROR: invalid position\n"
        reply = ipc(sock, "get-transition")
        assert reply == "TRANSITION: wave enabled 0.50 angle=120 pos=0.00,0.00\n", repr(reply)
    help_text = subprocess.run([BINARY, "--help"], capture_output=True, text=True).stdout
    for option in ("--ipc-socket", "--transition-type", "--cache-size",
                   "--transition-angle", "--transition-pos"):
        assert option in help_text, option


CHECKS = [check_instant_change, check_ipc_contract, check_left_wipe, check_center_round, check_outer,
          check_fade_midway, check_old_image_aspect, check_no_black_flash,
          check_failed_change_keeps_image, check_rapid_changes, check_video_change_ignores_transition]

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
