"""Capture gSlapper playing one of the demo's video segments.

Usage: capture_video.py <gslapper-src> <gslapper-binary> <video> <out-dir> --name NAME
                        [--slow N] [--length SECS] [--gpu-device DEV]

grim can't grab 1080p at 60 fps (about 35 fps with software rendering), so
feed gSlapper a copy of the clip re-timed N times slower (ffmpeg
setpts=N*PTS; the frames themselves are unchanged) and pass --slow N. Each
rendered frame is then on screen long enough to be grabbed several times;
only the first grab of each is kept, with its capture time, so
build_demo.py can play the clip back at its real speed whatever its frame
rate. --gpu-device sets a DRM render node for a GPU-rendered sway (e.g.
/dev/dri/renderD128 on Intel/Mesa); software rendering is enough at 8x.

Writes <out-dir>/<NAME>/NNNN.ppm, times.txt (first-grab time per kept frame,
seconds, frame 0 = 0.0) and SLOW, covering --length seconds of clip from
0.5 s after it appears.
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path

WIDTH, HEIGHT = 1920, 1080
SETTLE = 0.5


def grab(env, path):
    for _ in range(3):
        if subprocess.run(["grim", "-t", "ppm", str(path)], env=env).returncode == 0:
            return
    raise RuntimeError(f"grim failed for {path}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("src", type=Path)
    parser.add_argument("binary", type=Path)
    parser.add_argument("video", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--name", required=True)
    parser.add_argument("--slow", type=float, default=1.0)
    parser.add_argument("--length", type=float, default=2.5)
    parser.add_argument("--gpu-device")
    args = parser.parse_args()

    sys.path.insert(0, str(args.src / "tests"))
    import headless

    if args.gpu_device:
        spawn = headless.HeadlessSway.spawn

        def gpu_spawn(self, argv, name=None):
            # Swap the harness's software renderer for the GPU before sway starts
            if name == "sway":
                self.env["WLR_RENDERER"] = "gles2"
                self.env["WLR_RENDER_DRM_DEVICE"] = args.gpu_device
                self.env.pop("LIBGL_ALWAYS_SOFTWARE", None)
            return spawn(self, argv, name)

        headless.HeadlessSway.spawn = gpu_spawn

    frames = args.out / args.name
    frames.mkdir(parents=True, exist_ok=True)
    (frames / "SLOW").write_text(f"{args.slow}\n")
    with headless.HeadlessSway(width=WIDTH, height=HEIGHT) as sway:
        sock = sway.root / "demo.sock"
        player = sway.spawn([str(args.binary), "--no-save-state", "-o", "fill loop no-audio", "-r", "60",
                             "-l", "overlay", "--cache-size", "256", "-I", str(sock), "HEADLESS-1",
                             str(args.video)], name="gslapper")
        headless.wait_for_socket(sock, process=player)
        deadline = time.monotonic() + 30
        # The background before playback is black
        while max(headless.capture(sway.env).pixel(WIDTH // 2, HEIGHT // 2)) < 12:
            if time.monotonic() > deadline:
                raise RuntimeError("video never appeared:\n" + sway.log("gslapper")[-1500:])
            time.sleep(0.05)
        time.sleep(SETTLE * args.slow)
        stamps, grabs, worst_gap = [], 0, 0.0
        previous, last_grab = None, None
        t0 = time.monotonic()
        while time.monotonic() - t0 < args.length * args.slow:
            now = time.monotonic() - t0
            if last_grab is not None:
                worst_gap = max(worst_gap, now - last_grab)
            last_grab = now
            path = frames / f"{len(stamps):04d}.ppm"
            grab(sway.env, path)
            grabs += 1
            data = path.read_bytes()
            if data == previous:
                path.unlink()
                continue
            previous = data
            stamps.append(now)
        (frames / "times.txt").write_text("".join(f"{t - stamps[0]:.6f}\n" for t in stamps))
        print(f"{args.name}: {grabs} grabs, {len(stamps)} distinct frames, "
              f"worst grab gap {worst_gap * 1000:.0f} ms ({worst_gap / args.slow * 1000:.1f} ms of clip)")


if __name__ == "__main__":
    main()
