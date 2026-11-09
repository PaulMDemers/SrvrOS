import unittest
from capture_history import capture_history


def line(name, values):
    return (name + ': ' + ' '.join('%s=%s' % item for item in values.items()) + '\n').encode()


class TestCaptureHistory(unittest.TestCase):
    def fixture(self, count=3, final=1, at=1000):
        blob = line('desktop-capture-history', dict(version=1, snapshot_us=at, completed=count,
                    count=count, dropped=0, invalid=0, final=final))
        for i in range(count):
            r = dict(version=1, snapshot_us=at, index=i, id=i+1, pid=10+i,
                     started_us=100+i*200, finished_us=200+i*200,
                     retired_us=210+i*200, result=1, status=0)
            r.update({'p%d_us' % n: n for n in range(1, 13)})
            r['final'] = final
            blob += line('desktop-capture-completion', r)
        return blob

    def test_all_three_not_just_current_and_slowest(self):
        r = capture_history([self.fixture(), self.fixture()])
        self.assertEqual([c['id'] for c in r['captures']], [1, 2, 3])
        self.assertEqual([c['pid'] for c in r['captures']], [10, 11, 12])
        self.assertEqual(r['median_us'], 100)
        self.assertEqual(r['captures'][1]['phase_us'], 78)

    def test_periodic_is_not_final(self):
        with self.assertRaises(ValueError):
            capture_history([self.fixture(final=0)])
        self.assertEqual(capture_history([self.fixture(count=1, final=0, at=800), self.fixture()])['completed'], 3)
        with self.assertRaises(ValueError):
            capture_history([self.fixture(count=2, at=1200), self.fixture()])

    def test_reject_incomplete_dropped_and_failures(self):
        for a, b in [(b'completed=3', b'completed=2'), (b'count=3', b'count=2'),
                     (b'dropped=0', b'dropped=1'), (b'invalid=0', b'invalid=1'),
                     (b'result=1', b'result=-1'), (b'status=0', b'status=2'),
                     (b'index=1', b'index=0'), (b'id=2 pid=11', b'id=1 pid=11'),
                     (b'id=2 pid=11', b'id=2 pid=10'), (b'pid=10', b'pid=0')]:
            with self.subTest(change=b), self.assertRaises(ValueError):
                capture_history([self.fixture().replace(a, b)])
        lines = self.fixture().splitlines(keepends=True)
        for n in range(len(lines)):
            with self.subTest(missing=n), self.assertRaises(ValueError):
                capture_history([b''.join(lines[:n] + lines[n+1:])])

    def test_reject_clocks_overlap_and_phase_sum(self):
        for a, b in [(b'started_us=100', b'started_us=0'),
                     (b'finished_us=200', b'finished_us=99'),
                     (b'retired_us=210', b'retired_us=199'),
                     (b'retired_us=610', b'retired_us=1001'),
                     (b'started_us=300', b'started_us=209'),
                     (b'p12_us=12', b'p12_us=100')]:
            with self.subTest(change=b), self.assertRaises(ValueError):
                capture_history([self.fixture().replace(a, b)])

    def test_reject_schema_and_conflicts(self):
        for a, b in [(b'version=1', b'version=2'),
                     (b'pid=10', b'pid=18446744073709551616'),
                     (b'id=1 pid=10', b'id=4294967296 pid=10'),
                     (b'final=1', b'final=2'), (b'result=1', b'result=2'),
                     (b'p1_us=1', b'p1_us=-1'),
                     (b'completed=3 count=3', b'completed=3 extra=0 count=3')]:
            with self.subTest(change=b), self.assertRaises(ValueError):
                capture_history([self.fixture().replace(a, b)])
        with self.assertRaises(ValueError):
            capture_history([self.fixture(), self.fixture().replace(b'pid=11', b'pid=13')])
        with self.assertRaises(ValueError):
            capture_history([self.fixture() + self.fixture().splitlines()[1] + b' extra=0\n'])

    def test_bounded_ordinary_expectation(self):
        self.assertEqual(capture_history([self.fixture(count=0)], expected=0)['captures'], [])
        self.assertEqual(capture_history([self.fixture(count=1)], expected=1)['completed'], 1)
        for n in [-1, 4, '3', True]:
            with self.assertRaises(ValueError):
                capture_history([self.fixture()], expected=n)


if __name__ == '__main__':
    unittest.main()
