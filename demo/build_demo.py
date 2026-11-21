"""Assemble the captured frames into the README loop and encode it as WebP.

Usage: build_demo.py <frames-dir> <out.webp> [--quality Q]

<frames-dir> holds the output of capture_transitions.py and capture_video.py.
Each transition (in ORDER) is trimmed to its visible motion and resampled to
60 fps at the capture's SCALE; stills hold between transitions. Each video
segment is resampled from its kept frames' capture times at its SLOW factor,
anchored on its first complete frame and sampling tick centres, so it plays at its real speed whatever its frame rate.
"""
import argparse
import shutil
import subprocess
from pathlib import Path

from timeline import hold, link_sequence, resample, trim_transition, video_ticks

FPS = 60
HOLD = 0.45
VIDEO_SECONDS = 2.0
VIDEOS = ["video-1", "video-2", "video-3"]


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
    for name in (root / "ORDER").read_text().split():
        frames, times = frames_and_times(root / name)
        start, end = trim_transition(frames)
        picks = resample(times[start:end + 1], scale, FPS)
        sequence += hold(frames[start], HOLD, FPS)
        sequence += [frames[start + i] for i in picks]
        last = frames[end]
        print(f"{name}: {len(picks)} frames ({len(picks) / FPS:.2f} s)")
    sequence += hold(last, HOLD, FPS)

    needed = round(VIDEO_SECONDS * FPS)
    for name in VIDEOS:
        frames, times = frames_and_times(root / name)
        slow = float((root / name / "SLOW").read_text())
        picks = video_ticks(times, slow, needed, FPS)
        sequence += [frames[i] for i in picks]
        print(f"{name}: {needed} frames ({VIDEO_SECONDS:.2f} s) from {len(set(picks))} distinct")
    return sequence


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("frames", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--quality", type=int, default=90)
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
