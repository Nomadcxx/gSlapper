"""Turn captured gSlapper frames into an exact 60 fps frame list."""
import bisect
import os
from pathlib import Path


def resample(timestamps, scale, fps=60, offset=0.0):
    """Pick one captured frame per output tick.

    timestamps[i] is when frame i was grabbed (seconds, ascending). The
    capture ran `scale` times slower than playback, so output tick k shows
    capture time timestamps[0] + (k + offset) * scale / fps, using the latest
    frame grabbed at or before that moment. offset=0.5 samples the middle of
    each tick, which keeps grab jitter at frame boundaries from choosing the
    wrong frame.
    """
    t0 = timestamps[0]
    ticks = round((timestamps[-1] - t0) / scale * fps)
    return [bisect.bisect_right(timestamps, t0 + (k + offset) * scale / fps) - 1 for k in range(ticks)]


def video_ticks(timestamps, slow, count, fps=60):
    """Frame indices for `count` ticks of a video captured `slow` times slower.

    timestamps are first-grab times of the distinct frames. Frame 0 was
    already partway through its time on screen when capture began, so the
    timeline is anchored on frame 1, the first complete frame; sampling tick
    centres from there keeps each tick inside a single frame.
    """
    picks = resample(timestamps[1:], slow, fps, offset=0.5)
    if len(picks) < count:
        raise ValueError(f"{len(picks)} ticks captured, need {count}")
    return [i + 1 for i in picks[:count]]


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


def distinct_frames(frames):
    """Indices where the picture changes: the first grab of each rendered frame.

    Used for the video segment, captured from a slowed copy of the clip so
    every gSlapper frame is grabbed at least once."""
    keep = []
    previous = None
    for i, frame in enumerate(frames):
        data = frame.read_bytes()
        if data != previous:
            keep.append(i)
            previous = data
    return keep


def hold(frame, seconds, fps=60):
    return [frame] * round(seconds * fps)


def link_sequence(frames, out_dir):
    """Symlink frames into out_dir as 000000.ppm, 000001.ppm, ... for ffmpeg."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for i, frame in enumerate(frames):
        os.symlink(Path(frame).resolve(), out_dir / f"{i:06d}.ppm")
    return len(frames)
