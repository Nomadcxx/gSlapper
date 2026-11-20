"""Assemble the captured frames into the README loop and encode it as WebP.

Usage: build_demo.py <frames-dir> <out.webp> [--quality Q]

<frames-dir> holds the output of capture_transitions.py and capture_video.py.
Each transition is trimmed to its visible motion and resampled to 60 fps at
the capture's SCALE; stills hold between transitions; the video segment is
its distinct rendered frames, one per 60 fps tick.
"""
import argparse
import shutil
import subprocess
from pathlib import Path

from timeline import distinct_frames, hold, link_sequence, resample, trim_transition

FPS = 60
HOLD = 0.9
VIDEO_SECONDS = 4.0
TRANSITIONS = ["wave", "grow", "center", "wipe"]


def frames_and_times(directory):
    frames = sorted(directory.glob("*.ppm"))
    times = [float(t) for t in (directory / "times.txt").read_text().split()]
    if len(frames) != len(times):
        raise ValueError(f"{directory}: {len(frames)} frames but {len(times)} timestamps")
    return frames, times


def build_sequence(root):
    scale = float((root / "SCALE").read_text())
    sequence = []
    last = None
    for name in TRANSITIONS:
        frames, times = frames_and_times(root / name)
        start, end = trim_transition(frames)
        picks = resample(times[start:end + 1], scale, FPS)
        sequence += hold(frames[start], HOLD, FPS)
        sequence += [frames[start + i] for i in picks]
        last = frames[end]
        print(f"{name}: {len(picks)} frames ({len(picks) / FPS:.2f} s)")
    sequence += hold(last, HOLD, FPS)

    video = sorted((root / "video").glob("*.ppm"))
    needed = round(VIDEO_SECONDS * FPS)
    keep = distinct_frames(video)
    if len(keep) < needed:
        raise ValueError(f"video: {len(keep)} distinct frames, need {needed}")
    sequence += [video[i] for i in keep[:needed]]
    print(f"video: {needed} frames ({VIDEO_SECONDS:.2f} s)")
    return sequence


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("frames", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--quality", type=int, default=95)
    args = parser.parse_args()

    sequence = build_sequence(args.frames)
    seq_dir = args.frames / "seq"
    shutil.rmtree(seq_dir, ignore_errors=True)
    count = link_sequence(sequence, seq_dir)
    print(f"total: {count} frames ({count / FPS:.2f} s)")
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-framerate", str(FPS), "-i", str(seq_dir / "%06d.ppm"),
                    "-c:v", "libwebp_anim", "-lossless", "0", "-quality", str(args.quality),
                    "-compression_level", "6", "-loop", "0", str(args.out)], check=True)
    shutil.rmtree(seq_dir)
    print(f"wrote {args.out} ({args.out.stat().st_size / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
