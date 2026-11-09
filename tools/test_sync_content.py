import copy
import unittest
from sync_content import sync_content
import test_sync_timing as sync_fixture


def row(kind, **values):
    return ('sync-content-' + kind + ': ' + ' '.join('%s=%s' % item for item in values.items()) + '\n').encode()


class TestSyncContent(unittest.TestCase):
    def fixture(self):
        common = dict(version=1, snapshot_us=1000, pid=7, id=100)
        lines = [row('summary', **common, calls=2, bytes=65540, failures=0, final=1),
                 row('selection', **common, final=1),
                 row('worst', **common, slot=0, identity=10, inode=42, size=65536,
                     start_us=101, end_us=130, ok=1, dirty_after=0, final=1)]
        for role in range(6):
            lines.append(row('io', **common, role=role, reads=int(role == 3),
                             read_bytes=4096 if role == 3 else 0, read_us=4 if role == 3 else 0,
                             writes=int(role == 2), write_bytes=65536 if role == 2 else 0,
                             write_us=10 if role == 2 else 0, flushes=int(role == 0),
                             flush_us=2 if role == 0 else 0, failures=0, final=1))
        return sync_fixture.TestSyncTiming().fixture() + b''.join(lines)

    def test_complete_real_sync_join_and_duplicate(self):
        r = sync_content([self.fixture(), self.fixture()])
        self.assertEqual(r['status'], 'VALID')
        self.assertEqual(r['selection']['id'], 100)
        self.assertEqual(r['io_us'], 16)
        self.assertEqual(r['residual_us'], 13)
        self.assertEqual(r['worst']['identity'], 10)

    def test_binding_and_clock_reject(self):
        changes = [(b'id=100 calls=2 bytes=', b'id=99 calls=2 bytes='),
                   (b'pid=7 id=100 slot=', b'pid=8 id=100 slot='),
                   (b'start_us=101', b'start_us=99'), (b'end_us=130', b'end_us=141'),
                   (b'end_us=130', b'end_us=100'), (b'identity=10', b'identity=0'),
                   (b'slot=0 identity', b'slot=192 identity'), (b'size=65536', b'size=16777217'),
                   (b'ok=1 dirty_after=0', b'ok=0 dirty_after=0'),
                   (b'ok=1 dirty_after=0', b'ok=1 dirty_after=1')]
        for before, after in changes:
            with self.subTest(change=after), self.assertRaises(ValueError):
                sync_content([self.fixture().replace(before, after)])

    def test_schema_and_counter_reject(self):
        for before, after in [(b'sync-content-summary: version=1', b'sync-content-summary: version=2'),
                              (b'bytes=65540', b'bytes=18446744073709551616'),
                              (b'calls=2 bytes=', b'calls=193 bytes='),
                              (b'calls=2 bytes=', b'calls=0 bytes='),
                              (b'bytes=65540 failures=0', b'bytes=65540 failures=1'),
                              (b'read_us=4', b'read_us=40'), (b'role=5', b'role=6'),
                              (b'role=0 reads=0', b'role=0 reads=-1'),
                              (b'role=0 reads=0 read_bytes=0', b'role=0 reads=0 read_bytes=1'),
                              (b'flushes=0 flush_us=0 failures=0', b'flushes=0 flush_us=1 failures=0'),
                              (b'role=3 reads=1', b'role=3 reads=0')]:
            with self.subTest(change=after), self.assertRaises(ValueError):
                sync_content([self.fixture().replace(before, after)])
        for ending in (b' extra=1', b' final=1', b' '):
            bad = self.fixture().replace(b'calls=2 bytes=65540 failures=0 final=1\n',
                                         b'calls=2 bytes=65540 failures=0 final=1' + ending + b'\n')
            with self.assertRaises(ValueError):
                sync_content([bad])

    def test_partial_and_conflicting_reject(self):
        lines = self.fixture().splitlines(keepends=True)
        for prefix in (b'sync-content-summary:', b'sync-content-selection:', b'sync-content-worst:', b'sync-content-io:'):
            bad = b''.join(line for line in lines if not line.startswith(prefix))
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                sync_content([bad])
        with self.assertRaises(ValueError):
            sync_content([self.fixture(), self.fixture().replace(b'identity=10', b'identity=11')])
        with self.assertRaises(ValueError):
            sync_content([self.fixture().replace(b'sync-content-io: version=1', b'sync-content-io: version=1 broken=0', 1)])

    def test_selection_uses_same_worst_sync_and_tie_order(self):
        # Use an already-validated sync snapshot adapter to isolate selection.
        from sync_timing import sync_timing
        base = sync_timing([self.fixture()])
        second = copy.deepcopy(base['workers'][0])
        second['pid'] = second['record']['pid'] = second['worst']['pid'] = 8
        second['worst'].update(start_us=200, files_end_us=230, end_us=240)
        base['workers'].append(second)
        more = row('summary', version=1, snapshot_us=1000, pid=8, id=200,
                   calls=1, bytes=10, failures=0, final=1)
        self.assertEqual(sync_content([self.fixture() + more], base)['selection']['pid'], 7)
        second['worst']['files_end_us'] = 250
        with self.assertRaises(ValueError):
            sync_content([self.fixture() + more], base)
        second['worst']['files_end_us'] = 240
        self.assertEqual(sync_content([self.fixture() + more], base)['selection']['pid'], 7)

    def test_no_dirty_work_is_explicit(self):
        from sync_timing import sync_timing
        observed = sync_timing([self.fixture()])
        blob = row('summary', version=1, snapshot_us=1000, pid=7, id=100,
                   calls=0, bytes=0, failures=0, final=1)
        blob += row('selection', version=1, snapshot_us=1000, pid=0, id=0, final=1)
        with self.assertRaises(ValueError):
            sync_content([blob], observed)
        r = sync_content([blob], observed, require_work=False)
        self.assertIsNone(r['worst'])
        with self.assertRaises(ValueError):
            sync_content([blob + self.fixture()], observed, require_work=False)


if __name__ == '__main__':
    unittest.main()
