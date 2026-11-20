"""Turn captured gSlapper frames into an exact 60 fps frame list."""
import bisect
import os
from pathlib import Path


def resample(timestamps, scale, fps=60):
    """Pick one captured frame per output tick.

    timestamps[i] is when frame i was grabbed (seconds, ascending). The
    capture ran `scale` times slower than playback, so output tick k shows
    capture time timestamps[0] + k * scale / fps, using the latest frame
    grabbed at or before that moment.
    """
    t0 = timestamps[0]
    ticks = round((timestamps[-1] - t0) / scale * fps)
    return [bisect.bisect_right(timestamps, t0 + k * scale / fps) - 1 for k in range(ticks)]


def trim_transition(frames):
    """Return (start, end): the last frame still showing the old image and
    the first frame showing the finished new image."""
    first = frames[0].read_bytes()
    last = frames[-1].read_bytes()
    start = None
    for i, frame in enumerate(frames):
        if frame.read_bytes() != first:
            start = i - 1
            break
    if start is None:
        raise ValueError("no frame differs from the first; the transition never started")
    for i in range(start + 1, len(frames)):
        if frames[i].read_bytes() == last:
            return start, i
    raise ValueError("the transition never settled on the final frame")


def hold(frame, seconds, fps=60):
    return [frame] * round(seconds * fps)


def link_sequence(frames, out_dir):
    """Symlink frames into out_dir as 000000.ppm, 000001.ppm, ... for ffmpeg."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for i, frame in enumerate(frames):
        os.symlink(Path(frame).resolve(), out_dir / f"{i:06d}.ppm")
    return len(frames)
