#!/usr/bin/env python3
import unittest
from scheduler_activity import worker_activity


def block(at, ticks, switches, timeouts, signals, identity=1, extra=''):
    count = 1 + bool(extra)
    return (f'worker-snapshot: us={at} count={count} version=1\n'
            f'worker: us={at} slot=0 id={identity} name=worker state=3 ticks={ticks} switches={switches} timeouts={timeouts} signals={signals}\n'
            f'{extra}worker-snapshot-end: us={at} count={count}\n')


class Activity(unittest.TestCase):
    def setUp(self):
        self.text = ''.join(block(at, i, i*200, i*100, i*50) for i, at in
                            enumerate((10_000_000, 20_000_000, 30_000_000)))

    def test_delta(self):
        result = worker_activity(self.text, 1, 40_000_000)
        self.assertEqual(result['covered_us'], 20_000_000)
        self.assertEqual(result['checkpoints'], 3)
        self.assertEqual(result['workers'][0]['switches'], 400)
        self.assertEqual(result['workers'][0]['timeout_wakes_per_second'], 10)
        self.assertEqual(result['workers'][0]['signal_wakes_per_second'], 5)

    def test_expanded_slots(self):
        expanded = self.text.replace('version=1', 'version=2').replace('slot=0', 'slot=95')
        self.assertIsNotNone(worker_activity(expanded, 1, 40_000_000))
        self.assertIsNone(worker_activity(expanded.replace('slot=95', 'slot=256'), 1, 40_000_000))
        self.assertIsNone(worker_activity(expanded.replace('version=2', 'version=1'), 1, 40_000_000))

    def test_window_excludes_later_work(self):
        later = block(50_000_000, 9000, 9000, 9000, 9000)
        self.assertEqual(worker_activity(self.text+later, 1, 40_000_000),
                         worker_activity(self.text, 1, 40_000_000))
        self.assertIsNone(worker_activity(self.text, 15_000_000, 40_000_000))

    def test_incomplete_and_corrupt(self):
        for bad in (self.text.rsplit('worker-snapshot-end:', 1)[0],
                    self.text.replace('count=1 version', 'count=2 version', 1),
                    self.text.replace('id=1', 'id=0', 1),
                    self.text.replace('state=3', 'state=9', 1),
                    self.text.replace('ticks=2', 'ticks=0'),
                    self.text.replace('timeouts=200', 'timeouts=99'),
                    self.text.replace('slot=0', 'slot=1', 1),
                    self.text.replace('name=worker', 'name=other', 1),
                    self.text.replace('switches=400', f'switches={2**64}'),
                    self.text+self.text,
                    self.text.replace('worker: us=10000000', 'worker: us=9999999'),
                    self.text.replace('version=1', 'version=3')):
            with self.subTest(bad=bad):
                self.assertIsNone(worker_activity(bad, 1, 40_000_000))

    def test_reused_slot_not_combined(self):
        text = self.text.replace('us=30000000 slot=0 id=1', 'us=30000000 slot=0 id=2')
        result = worker_activity(text, 1, 40_000_000)
        self.assertEqual(result['workers'], [])
        self.assertEqual(result['partial_lifetime_ids'], [1, 2])

    def test_duplicate_and_missing_rows(self):
        row = 'worker: us=10000000 slot=0 id=1 name=worker state=3 ticks=0 switches=0 timeouts=0 signals=0\n'
        for duplicate in (row, row.replace('id=1', 'id=2'), row.replace('slot=0', 'slot=1')):
            text = block(10_000_000, 0, 0, 0, 0, extra=duplicate)+self.text.split('worker-snapshot-end: us=10000000 count=1\n')[1]
            self.assertIsNone(worker_activity(text, 1, 40_000_000))
        self.assertIsNone(worker_activity(self.text.replace(row, ''), 1, 40_000_000))


if __name__ == '__main__':
    unittest.main()
