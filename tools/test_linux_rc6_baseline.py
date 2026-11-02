import unittest
from linux_rc6_baseline import interval, summarize


def row(seq, ms, ns):
    return dict(seq=seq, rc6_ms=ms, raw_ns=[ns, ns+1000],
                mono_ns=[ns, ns+2000])


class BaselineTests(unittest.TestCase):
    def test_full_windows_and_quantization(self):
        rows = [row(i, 1000+i*10001, 1_000_000+i*10_000_000_000) for i in range(5)]
        r = summarize(rows, 2)
        self.assertFalse(r['acceptance'])
        self.assertEqual(len(r['windows']), 2)
        self.assertEqual(len(r['intervals']), 4)
        self.assertEqual(r['intervals'][0]['raw']['excess_beyond_2ms_ns'], 0)

    def test_excess_remains_observation(self):
        r = interval(row(0, 0, 1_000_000), row(1, 10020, 10_001_000_000))
        self.assertGreater(r['excess_beyond_2ms_ns'], 17_000_000)

    def test_reset_clock_and_missing_rows_refused(self):
        a, b = row(0, 100, 1_000_000), row(1, 90, 2_000_000)
        with self.assertRaises(ValueError): interval(a, b)
        with self.assertRaises(ValueError): interval(a, a)
        with self.assertRaises(ValueError): summarize([a, b], 2)
        with self.assertRaises(ValueError): summarize([a, row(2, 110, 2_000_000)], 1)
        a['mono_ns'] = [2, 1]
        with self.assertRaises(ValueError): summarize([a, row(1, 110, 2_000_000)], 1)


if __name__ == '__main__':
    unittest.main()
