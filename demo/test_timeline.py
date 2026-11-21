import tempfile
import unittest
from pathlib import Path

from demo.timeline import distinct_frames, hold, link_sequence, resample, trim_transition, video_ticks


class TimelineTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def frames(self, contents):
        paths = []
        for i, data in enumerate(contents):
            path = self.tmp / f"{i:04d}.ppm"
            path.write_bytes(data)
            paths.append(path)
        return paths

    def test_resample_regular(self):
        # 4x slowed, 240 Hz capture over 4.8 s -> 72 ticks at 60 fps
        ts = [i / 240 for i in range(1153)]
        idx = resample(ts, scale=4.0)
        self.assertEqual(len(idx), 72)
        self.assertEqual(idx[0], 0)
        self.assertEqual(idx[1], 16)  # 4/60 s later = 16 capture frames

    def test_resample_irregular_timestamps(self):
        ts = [0.0, 0.010, 0.050, 0.051, 0.120]
        # ticks at 0, .02, .04, .06, .08, .10 s -> latest frame at or before each, never a future one
        self.assertEqual(resample(ts, scale=1.0, fps=50), [0, 1, 1, 3, 3, 3])

    def test_resample_tick_centres_keep_source_cadence(self):
        # 30 fps clip slowed 8x: distinct frames first grabbed every 8/30 s, a few ms late.
        # Sampling each tick's centre shows every frame for two 60 fps ticks.
        ts = [k * 8 / 30 + 0.004 for k in range(10)]
        self.assertEqual(resample(ts, scale=8.0, offset=0.5)[:8], [0, 0, 1, 1, 2, 2, 3, 3])

    def test_video_ticks_anchor_on_first_complete_frame(self):
        # 60 fps clip slowed 8x; capture began 70 ms before frame 1 replaced frame 0 (as in the real video-1 run).
        # Anchoring on frame 1 puts every tick centre mid-frame: one tick per frame, none doubled.
        ts = [0.0] + [0.070 + k * 8 / 60 for k in range(12)]
        self.assertEqual(video_ticks(ts, slow=8.0, count=10), list(range(1, 11)))

    def test_trim_transition(self):
        frames = self.frames([b"A", b"A", b"A", b"B", b"C", b"D", b"D"])
        self.assertEqual(trim_transition(frames), (2, 5))

    def test_trim_transition_no_change_raises(self):
        frames = self.frames([b"A", b"A", b"A"])
        with self.assertRaises(ValueError):
            trim_transition(frames)

    def test_distinct_frames_collapses_repeat_grabs(self):
        # each rendered frame was grabbed one to three times; only changes count
        frames = self.frames([b"A", b"A", b"B", b"C", b"C", b"C", b"A", b"D"])
        self.assertEqual(distinct_frames(frames), [0, 2, 3, 6, 7])

    def test_sequence_frame_count_exact(self):
        still, other = self.frames([b"S", b"T"])
        transition = [still] * 36 + [other] * 36
        seq = hold(still, 0.9) + transition + hold(other, 0.9)
        out = self.tmp / "seq"
        self.assertEqual(link_sequence(seq, out), 180)
        self.assertEqual(len(list(out.iterdir())), 180)
        self.assertEqual((out / "000000.ppm").resolve(), still.resolve())
        self.assertEqual((out / "000179.ppm").resolve(), other.resolve())


if __name__ == "__main__":
    unittest.main()
