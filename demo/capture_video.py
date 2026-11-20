"""Capture gSlapper playing the demo's video segment.

Usage: capture_video.py <gslapper-src> <gslapper-binary> <video> <out-dir> [--slow N] [--gpu-device DEV]

grim can't grab 1080p at 60 fps (about 30 fps even on a GPU-rendered
compositor), so feed gSlapper a copy of the clip re-timed N times slower
(ffmpeg setpts=N*PTS; the frames themselves are unchanged) and pass
--slow N. Every rendered frame is then grabbed at least once, and
build_demo.py keeps the first grab of each. --gpu-device sets the DRM render
node for a GPU-rendered sway (e.g. /dev/dri/renderD128 on Intel/Mesa).

Writes <out-dir>/video/NNNN.ppm and times.txt (capture time per frame,
seconds, frame 0 = 0.0), covering 5 s of clip from 1 s after it appears.
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path

WIDTH, HEIGHT = 1920, 1080
SETTLE, LENGTH = 1.0, 5.0


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
    parser.add_argument("--slow", type=float, default=1.0)
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

    frames = args.out / "video"
    frames.mkdir(parents=True, exist_ok=True)
    with headless.HeadlessSway(width=WIDTH, height=HEIGHT) as sway:
        sock = sway.root / "demo.sock"
        player = sway.spawn([str(args.binary), "--no-save-state", "-o", "fill loop no-audio", "-r", "60",
                             "-l", "overlay", "--cache-size", "256", "-I", str(sock), "HEADLESS-1",
                             str(args.video)], name="gslapper")
        headless.wait_for_socket(sock, process=player)
        deadline = time.monotonic() + 15
        # The background before playback is black; the clip's darkest frame is not
        while max(headless.capture(sway.env).pixel(WIDTH // 2, HEIGHT // 2)) < 12:
            if time.monotonic() > deadline:
                raise RuntimeError("video never appeared:\n" + sway.log("gslapper")[-1500:])
            time.sleep(0.05)
        time.sleep(SETTLE * args.slow)
        stamps = []
        t0 = time.monotonic()
        while time.monotonic() - t0 < LENGTH * args.slow:
            stamps.append(time.monotonic() - t0)
            grab(sway.env, frames / f"{len(stamps) - 1:04d}.ppm")
        (frames / "times.txt").write_text("".join(f"{t - stamps[0]:.6f}\n" for t in stamps))
        rate = (len(stamps) - 1) / (stamps[-1] - stamps[0])
        print(f"video: {len(stamps)} frames, {rate:.1f} fps captured")


if __name__ == "__main__":
    main()
