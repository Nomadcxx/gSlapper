import tempfile
import unittest
from pathlib import Path

from demo.timeline import hold, link_sequence, resample, trim_transition


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

    def test_trim_transition(self):
        frames = self.frames([b"A", b"A", b"A", b"B", b"C", b"D", b"D"])
        self.assertEqual(trim_transition(frames), (2, 5))

    def test_trim_transition_no_change_raises(self):
        frames = self.frames([b"A", b"A", b"A"])
        with self.assertRaises(ValueError):
            trim_transition(frames)

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
