import unittest
from loop_timing import FIELDS, PREFIX, WAIT_FIELDS, SCHED_FIELDS, validate_loop_timing


def receipt(**changes):
    row = dict.fromkeys(FIELDS, 0)
    row.update(version=1, samples=1000, seq=2, start_us=100, end_us=559100,
               gap_us=559000, over_limit=1, cycle=4, pid=13, phase=3,
               end_cycle=4, end_pid=13, end_phase=3, control_us=1000,
               render_us=550000, wait_us=8000, final=1)
    row.update(changes)
    return PREFIX+b' '.join(f'{key}={row[key]}'.encode() for key in FIELDS)+b'\n'


def wait_receipt(**changes):
    row = dict.fromkeys(WAIT_FIELDS, 0)
    row.update(seq=2, valid=1, begin_us=200, sleep_begin_us=220,
               sleep_end_us=540790, end_us=540810, final=1)
    row.update(changes)
    return b'desktop-loop-wait: '+b' '.join(f'{k}={row[k]}'.encode() for k in WAIT_FIELDS)+b'\n'


def sched_receipt(**changes):
    row = dict.fromkeys(SCHED_FIELDS, 0)
    row.update(seq=2, version=1, start_us=210, end_us=540800, end_tick=54,
               blocked_us=10000, ready_us=520000, blocks=1, timeouts=1,
               dispatches=1, exclusions=2, max_ready_us=520000,
               wake_us=10210, run_us=530210, wake_tick=1, deadline_tick=1,
               reason=2, ready_exclusions=2, final=1)
    row.update(changes)
    return b'desktop-loop-sched: '+b' '.join(f'{k}={row[k]}'.encode() for k in SCHED_FIELDS)+b'\n'


def delayed_receipt(**changes):
    return receipt(version=2, end_us=540900, gap_us=540800, control_us=63,
                   render_us=0, wait_us=540737, **changes)


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

    def test_retained_wait_attribution(self):
        periodic = delayed_receipt(final=0)+wait_receipt(final=0)+sched_receipt(final=0)
        final = delayed_receipt()+wait_receipt()+sched_receipt()
        row = validate_loop_timing([periodic+final, final], 540800)
        self.assertEqual(row['scheduler']['ready_us'], 520000)
        self.assertEqual(row['scheduler']['blocked_us'], 10000)
        self.assertEqual(row['wait']['sleep_begin_us'], 220)
        # A valid GUI failure followed by the existing one-tick fallback.
        fallback = wait_receipt(gui_begin_us=215, gui_end_us=219, gui_result=-1)
        validate_loop_timing([delayed_receipt()+fallback+sched_receipt()])
        # Early exit before WAIT has explicit zeroed companion records.
        raw = receipt(version=2, wait_us=0, render_us=558000)
        w = dict.fromkeys(WAIT_FIELDS, 0); w.update(seq=2, final=1)
        q = dict.fromkeys(SCHED_FIELDS, 0); q.update(seq=2, final=1)
        raw += b'desktop-loop-wait: '+b' '.join(f'{k}={v}'.encode() for k,v in w.items())+b'\n'
        raw += b'desktop-loop-sched: '+b' '.join(f'{k}={v}'.encode() for k,v in q.items())+b'\n'
        validate_loop_timing([raw])

    def test_invalid_wait_attribution_refused(self):
        for wait, sched in [(b'', sched_receipt()), (wait_receipt(), b''),
                (wait_receipt(seq=3), sched_receipt()),
                (wait_receipt(valid=0), sched_receipt()),
                (wait_receipt(end_us=540901), sched_receipt()),
                (wait_receipt(sleep_begin_us=0), sched_receipt()),
                (wait_receipt(gui_begin_us=215, gui_end_us=219), sched_receipt()),
                (wait_receipt(), sched_receipt(invalid=1)),
                (wait_receipt(), sched_receipt(blocked_us=30000)),
                (wait_receipt(), sched_receipt(run_us=530211)),
                (wait_receipt(), sched_receipt(signals=1)),
                (wait_receipt(), sched_receipt(deadline_tick=2)),
                (wait_receipt(), sched_receipt(ready_exclusions=3)),
                (wait_receipt(), sched_receipt(final=0)),
                (wait_receipt()+wait_receipt(sleep_end_us=540788), sched_receipt())]:
            with self.subTest(wait=wait, sched=sched), self.assertRaises(AssertionError):
                validate_loop_timing([delayed_receipt()+wait+sched])

    def test_exact_limit_is_not_over(self):
        row = validate_loop_timing([receipt(gap_us=500000, end_us=500100,
                    render_us=491000, over_limit=0)], 500000)
        self.assertEqual(row['over_limit'], 0)


if __name__ == '__main__':
    unittest.main()
