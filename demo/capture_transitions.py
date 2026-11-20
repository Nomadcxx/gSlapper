"""Capture the README loop's transitions from gSlapper in headless sway.

Usage: capture_transitions.py <gslapper-src> <gslapper-binary> <media-dir> <out-dir>

Software rendering can't grab 1080p at 60 fps, so each transition runs SCALE
times slower than it appears in the demo. Transition progress is elapsed time
over duration, and the shader's only animated input is that progress, so the
slowed run sped back up is frame-for-frame the real thing.

Writes <out-dir>/<name>/NNNN.ppm and times.txt (capture time per frame,
seconds, frame 0 = 0.0) for each transition, and <out-dir>/SCALE.
"""
import subprocess
import sys
import time
from pathlib import Path

SCALE = 4.0
ON_SCREEN = 1.2
WIDTH, HEIGHT = 1920, 1080
LEAD, TAIL = 0.3, 0.6

IMAGES = ["19 - BaDWxwe.jpg", "wp14824417-1920x1080-wallpapers.webp", "148 - DJPJkF0.jpg"]
# (name, type, extra IPC commands, image index shown after the change)
TRANSITIONS = [
    ("wave", "wave", ["set-transition-angle 30"], 1),
    ("grow", "grow", ["set-transition-pos top-left"], 2),
    ("center", "center", [], 0),
    ("wipe", "wipe", ["set-transition-angle 30"], 1),
]


def grab(env, path):
    for _ in range(3):
        if subprocess.run(["grim", "-t", "ppm", str(path)], env=env).returncode == 0:
            return
    raise RuntimeError(f"grim failed for {path}")


def main():
    src, binary, media, out = (Path(a) for a in sys.argv[1:5])
    sys.path.insert(0, str(src / "tests"))
    import headless

    images = [media / name for name in IMAGES]
    out.mkdir(parents=True, exist_ok=True)
    (out / "SCALE").write_text(f"{SCALE}\n")

    with headless.HeadlessSway(width=WIDTH, height=HEIGHT) as sway:
        sock = sway.root / "demo.sock"
        player = sway.spawn([str(binary), "--no-save-state", "-o", "fill", "-r", "60", "-l", "overlay",
                             "--cache-size", "256", "-I", str(sock), "HEADLESS-1", str(images[0])],
                            name="gslapper")
        headless.wait_for_socket(sock, process=player)
        for image in images[1:]:
            reply = headless.ipc(sock, f"preload {image}")
            assert reply.startswith("OK"), reply
        time.sleep(1.0)

        for name, kind, extra, target in TRANSITIONS:
            for command in [f"set-transition {kind}", f"set-transition-duration {SCALE * ON_SCREEN}", *extra]:
                reply = headless.ipc(sock, command)
                assert reply.startswith("OK"), f"{command}: {reply}"
            frames = out / name
            frames.mkdir(exist_ok=True)
            stamps = []
            t0 = time.monotonic()
            changed = False
            while time.monotonic() - t0 < LEAD + SCALE * ON_SCREEN + TAIL:
                if not changed and time.monotonic() - t0 >= LEAD:
                    reply = headless.ipc(sock, f"change {images[target]}")
                    assert reply.startswith("OK"), reply
                    changed = True
                stamps.append(time.monotonic() - t0)
                grab(sway.env, frames / f"{len(stamps) - 1:04d}.ppm")
            (frames / "times.txt").write_text("".join(f"{t - stamps[0]:.6f}\n" for t in stamps))
            rate = (len(stamps) - 1) / (stamps[-1] - stamps[0])
            print(f"{name}: {len(stamps)} frames, {rate:.1f} fps captured, {rate * SCALE:.0f} fps effective")
            time.sleep(0.5)


if __name__ == "__main__":
    main()
