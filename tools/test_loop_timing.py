import unittest
from loop_timing import FIELDS, PREFIX, validate_loop_timing


def receipt(**changes):
    row = dict.fromkeys(FIELDS, 0)
    row.update(version=1, samples=1000, seq=2, start_us=100, end_us=559100,
               gap_us=559000, over_limit=1, cycle=4, pid=13, phase=3,
               end_cycle=4, end_pid=13, end_phase=3, control_us=1000,
               render_us=550000, wait_us=8000, final=1)
    row.update(changes)
    return PREFIX+b' '.join(f'{key}={row[key]}'.encode() for key in FIELDS)+b'\n'


class LoopTimingTests(unittest.TestCase):
    def test_rotation_and_overlap_keep_old_worst(self):
        early = receipt(samples=10, final=0)
        late = receipt(samples=1000, final=0)
        final = receipt()
        # The original gap event and first report have rotated out. Repeated
        # snapshots retain the original interval without inventing a new one.
        row = validate_loop_timing([late+final, final], 559000)
        self.assertEqual(row['seq'], 2)
        self.assertEqual(row['render_us'], 550000)
        self.assertEqual(validate_loop_timing([early+late+final], 559000), row)
        self.assertEqual(validate_loop_timing([late], require_final=False)['final'], 0)

    def test_bad_or_incomplete_proof_refused(self):
        bad = [b'', receipt(final=0), receipt()[:-20], receipt(invalid=1),
               receipt(end_us=559101), receipt(render_us=550001),
               receipt(seq=1001), receipt(over_limit=0), receipt(phase=4),
               receipt(gap_us=500000, end_us=500100, render_us=491000),
               receipt(final=0)+receipt(seq=1), receipt()+receipt(samples=1001),
               receipt(final=0)+receipt(cycle=5), receipt(version=2)]
        for raw in bad:
            with self.subTest(raw=raw[:120]), self.assertRaises(AssertionError):
                validate_loop_timing([raw])
        with self.assertRaises(AssertionError):
            validate_loop_timing([receipt()], 558999)

    def test_exact_limit_is_not_over(self):
        row = validate_loop_timing([receipt(gap_us=500000, end_us=500100,
                    render_us=491000, over_limit=0)], 500000)
        self.assertEqual(row['over_limit'], 0)


if __name__ == '__main__':
    unittest.main()
