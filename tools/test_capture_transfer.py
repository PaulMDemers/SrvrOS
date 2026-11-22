import unittest

from capture_history import capture_history
from capture_transfer import capture_transfer


def line(name, values):
    return (name + ': ' + ' '.join('%s=%s' % item for item in values.items()) + '\n').encode()


def change(blob, prefix, selected_index=0, **updates):
    rows = []
    for raw in blob.decode().splitlines():
        name, _, text = raw.partition(': ')
        fields = dict(item.split('=', 1) for item in text.split())
        if name == prefix and fields.get('index') == str(selected_index):
            fields.update({k: str(v) for k, v in updates.items()})
            raw = name + ': ' + ' '.join('%s=%s' % item for item in fields.items())
        rows.append(raw)
    return ('\n'.join(rows) + '\n').encode()


class TestCaptureTransfer(unittest.TestCase):
    def fixture(self, count=3, at=1000, final=1):
        blob = line('desktop-capture-history', dict(version=1, snapshot_us=at,
                    completed=count, count=count, dropped=0, invalid=0, final=final))
        for i in range(count):
            n = i * 300
            common = dict(version=1, snapshot_us=at, index=i, id=i+1, pid=42+i,
                          started_us=90+n, finished_us=190+n, retired_us=200+n)
            completion = dict(common, result=1, status=0)
            completion.update({'p%d_us' % phase: 8 if phase == 1 else 0 for phase in range(1, 13)})
            completion['final'] = final
            blob += line('desktop-capture-completion', completion)
            blob += line('desktop-capture-transfer', dict(common, enabled=1, valid=1,
                    invalid=0, total=4272, polls=4, budget_polls=0, begin_us=100+n,
                    end_us=190+n, poll_us=30, gap_us=60, gap_max_us=20, final=final))
            blob += line('desktop-capture-transfer-write', dict(common, write_calls=2,
                    write_ok=2, write_bytes=4272, short_writes=0, write_again=0,
                    write_intr=0, write_failed=0, write_first_us=101+n,
                    write_last_us=112+n, write_us=8, write_gap_us=3,
                    write_gap_max_us=3, request_done_us=105+n,
                    pixels_done_us=112+n, final=final))
            blob += line('desktop-capture-transfer-read', dict(common, read_calls=4,
                    read_bytes=128, read_again=0, read_intr=0, read_failed=0,
                    progress_frames=2, worker_receive_us=103+n,
                    worker_allocated_us=106+n, worker_read_done_us=111+n,
                    worker_final_us=180+n, final=final))
        return blob

    def test_complete_exact_history_and_duplicates(self):
        blob = self.fixture()
        result = capture_transfer([blob, blob], expected_pixels=4096)
        self.assertEqual(result['status'], 'VALID')
        self.assertEqual(result['completed'], 3)
        self.assertEqual([r['pid'] for r in result['captures']], [42, 43, 44])
        first = result['captures'][0]
        self.assertEqual(first['allocation_us'], 3)
        self.assertEqual(first['receive_us'], 5)
        self.assertEqual(first['final_observation_us'], 10)
        # A worker may read input before the writer resumes from its final write.
        self.assertLess(first['read']['worker_read_done_us'], first['write']['pixels_done_us'])
        self.assertEqual(capture_transfer([self.fixture(count=1)], expected=1)['completed'], 1)
        self.assertEqual(capture_transfer([self.fixture(count=0)], expected=0)['captures'], [])

    def test_periodic_not_a_final_substitute(self):
        with self.assertRaises(ValueError):
            capture_transfer([self.fixture(final=0)])
        result = capture_transfer([self.fixture(count=2, at=800, final=0), self.fixture()])
        self.assertEqual(result['completed'], 3)
        with self.assertRaises(ValueError):
            capture_transfer([self.fixture(at=800), self.fixture(count=2, at=1000)])

    def test_missing_extra_and_conflicting_companions(self):
        rows = self.fixture().splitlines(keepends=True)
        for n, row in enumerate(rows):
            if row.startswith(b'desktop-capture-transfer'):
                with self.subTest(missing=n), self.assertRaises(ValueError):
                    capture_transfer([b''.join(rows[:n]+rows[n+1:])])
        for prefix in ('desktop-capture-transfer', 'desktop-capture-transfer-write', 'desktop-capture-transfer-read'):
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                capture_transfer([self.fixture(), change(self.fixture(), prefix, pid=99)])
        extra = next(r for r in rows if r.startswith(b'desktop-capture-transfer:')).replace(b'index=0', b'index=3')
        with self.assertRaises(ValueError):
            capture_transfer([self.fixture()+extra])
        with self.assertRaises(ValueError):
            capture_transfer([b''.join(r for r in rows if not r.startswith(b'desktop-capture-transfer'))])

    def test_exact_schema_and_u64_flags(self):
        for before, after in [(b'desktop-capture-transfer:', b'desktop-capture-transfer-bogus:'),
                              (b'enabled=1 valid=1', b'enabled=1 valid=2'),
                              (b'enabled=1 valid=1', b'enabled=0 valid=1'),
                              (b'valid=1 invalid=0', b'valid=1 invalid=1'),
                              (b'polls=4', b'polls=18446744073709551616'),
                              (b'polls=4', b'polls=-1'),
                              (b'polls=4', 'polls=４'.encode()),
                              (b'budget_polls=0', b'budget_polls=0 extra=1')]:
            with self.subTest(change=after), self.assertRaises(ValueError):
                capture_transfer([self.fixture().replace(before, after)])
        for field, value in [('version', 2), ('final', 2), ('snapshot_us', 0)]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                capture_transfer([change(self.fixture(), 'desktop-capture-transfer', **{field: value})])

    def test_lifecycle_join_and_expectations(self):
        for prefix in ('desktop-capture-transfer', 'desktop-capture-transfer-write', 'desktop-capture-transfer-read'):
            for field, value in [('snapshot_us', 999), ('index', 3), ('id', 2), ('pid', 43),
                                 ('started_us', 91), ('finished_us', 191), ('retired_us', 201), ('final', 0)]:
                with self.subTest(prefix=prefix, field=field), self.assertRaises(ValueError):
                    capture_transfer([change(self.fixture(), prefix, **{field: value})])
        for expected in (-1, 4, True, '3'):
            with self.subTest(expected=expected), self.assertRaises(ValueError):
                capture_transfer([self.fixture()], expected=expected)
        for pixels in (0, -4, True, '4096', 3, 8*1024*1024):
            with self.subTest(pixels=pixels), self.assertRaises(ValueError):
                capture_transfer([self.fixture()], expected_pixels=pixels)
        with self.assertRaises(ValueError):
            capture_transfer([self.fixture()], expected_pixels=4100)
        history = capture_history([self.fixture()]); history['snapshot_us'] = 999
        with self.assertRaises(ValueError):
            capture_transfer([self.fixture()], history=history)

    def test_poll_and_write_partitions_and_bounds(self):
        header_changes = [dict(total=4273), dict(total=176), dict(polls=0), dict(polls=3),
                          dict(budget_polls=4), dict(budget_polls=1), dict(begin_us=89),
                          dict(end_us=191), dict(poll_us=31), dict(gap_us=61),
                          dict(gap_max_us=19), dict(gap_max_us=61)]
        for update in header_changes:
            with self.subTest(update=update), self.assertRaises(ValueError):
                capture_transfer([change(self.fixture(), 'desktop-capture-transfer', **update)])
        write_changes = [dict(write_calls=1), dict(write_calls=17), dict(write_ok=1),
                         dict(write_bytes=4271), dict(short_writes=3), dict(write_again=1),
                         dict(write_intr=1), dict(write_failed=1), dict(write_first_us=99),
                         dict(request_done_us=100), dict(request_done_us=113),
                         dict(pixels_done_us=111), dict(write_last_us=191),
                         dict(write_us=9), dict(write_gap_us=4), dict(write_gap_max_us=2),
                         dict(write_gap_max_us=4)]
        for update in write_changes:
            with self.subTest(update=update), self.assertRaises(ValueError):
                capture_transfer([change(self.fixture(), 'desktop-capture-transfer-write', **update)])

    def test_fragmentation_backpressure_and_retry(self):
        blob = change(self.fixture(), 'desktop-capture-transfer', polls=8, budget_polls=1,
                      gap_max_us=10)
        blob = change(blob, 'desktop-capture-transfer-write', write_calls=6, write_ok=4,
                      short_writes=2, write_again=1, write_intr=1, write_gap_max_us=1)
        blob = change(blob, 'desktop-capture-transfer-read', read_calls=8,
                      read_again=1, read_intr=1)
        self.assertEqual(capture_transfer([blob])['captures'][0]['write']['write_calls'], 6)
        # Attempts and successful calls must remain bounded independently.
        for update in [dict(write_ok=3), dict(write_calls=5), dict(short_writes=5)]:
            with self.subTest(update=update), self.assertRaises(ValueError):
                capture_transfer([change(blob, 'desktop-capture-transfer-write', **update)])

    def test_budget_lower_accounting_for_full_size_and_blocked_sends(self):
        # Four-per-poll admission cannot explain 1,026 successful writes with
        # no budget exhaustion and no interrupted/backpressured send polls.
        blob = change(self.fixture(), 'desktop-capture-transfer', total=4196528,
                      polls=300, budget_polls=0)
        blob = change(blob, 'desktop-capture-transfer-write', write_calls=1026,
                      write_ok=1026, write_bytes=4196528)
        blob = change(blob, 'desktop-capture-transfer-read', read_calls=300,
                      read_again=296)
        with self.assertRaises(ValueError):
            capture_transfer([blob])
        budgeted = change(blob, 'desktop-capture-transfer', budget_polls=256)
        self.assertEqual(capture_transfer([budgeted])['captures'][0]['write']['write_ok'], 1026)

        # A short-write stream can instead break on AGAIN/INTR: each such poll
        # admits at most three successes before that fourth-or-earlier attempt.
        blocked = change(self.fixture(), 'desktop-capture-transfer', polls=8,
                         budget_polls=0, gap_max_us=10)
        blocked = change(blocked, 'desktop-capture-transfer-write', write_calls=7,
                         write_ok=6, short_writes=5, write_again=1,
                         write_gap_max_us=1)
        blocked = change(blocked, 'desktop-capture-transfer-read', read_calls=8,
                         read_again=4)
        self.assertEqual(capture_transfer([blocked])['captures'][0]['write']['write_ok'], 6)
        unsupported = change(blocked, 'desktop-capture-transfer-write',
                             write_calls=9, write_ok=8)
        with self.assertRaises(ValueError):
            capture_transfer([unsupported])

    def test_response_frame_and_worker_clock_proofs(self):
        changes = [dict(read_calls=3), dict(read_bytes=127), dict(read_again=1),
                   dict(read_intr=5), dict(read_failed=1), dict(progress_frames=1),
                   dict(progress_frames=3), dict(worker_receive_us=100),
                   dict(worker_allocated_us=102), dict(worker_allocated_us=112),
                   dict(worker_read_done_us=112), dict(worker_final_us=110),
                   dict(worker_final_us=191)]
        for update in changes:
            with self.subTest(update=update), self.assertRaises(ValueError):
                capture_transfer([change(self.fixture(), 'desktop-capture-transfer-read', **update)])
        blob = change(self.fixture(), 'desktop-capture-transfer-read', worker_read_done_us=112)
        blob = change(blob, 'desktop-capture-completion', p1_us=9)
        self.assertEqual(capture_transfer([blob])['captures'][0]['worker_receive_phase_us'], 9)


if __name__ == '__main__':
    unittest.main()
