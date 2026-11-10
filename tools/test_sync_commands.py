import copy
import unittest
from sync_commands import sync_commands
from sync_content import sync_content
from sync_timing import sync_timing
import test_sync_content as content_fixture


def row(kind, **values):
    return ('sync-command-' + kind + ': ' + ' '.join('%s=%s' % item for item in values.items()) + '\n').encode()


def change(blob, predicate, **updates):
    lines = []
    for line in blob.decode().splitlines():
        if predicate(line):
            prefix, fields = line.split(': ', 1)
            values = dict(item.split('=', 1) for item in fields.split())
            values.update({k: str(v) for k, v in updates.items()})
            line = prefix + ': ' + ' '.join('%s=%s' % item for item in values.items())
        lines.append(line)
    return ('\n'.join(lines) + '\n').encode()

class TestSyncCommands(unittest.TestCase):
    def fixture(self):
        common = dict(version=1, snapshot_us=1000, pid=7, id=100, identity=10)
        lines = [row('snapshot', **common, valid=1, active=0, invalid=0, final=1)]
        for role in range(7):
            lines.append(row('role', **common, role=role, reads=0, read_bytes=0,
                             writes=int(role == 2), write_bytes=16384 if role == 2 else 0,
                             flushes=0, other=0, failed=0, csws=int(role == 2),
                             csw_us=2 if role == 2 else 0, over100ms=0, write512_commands=0,
                             write_csw_us=2 if role == 2 else 0, write512_csw_us=0,
                             write_over100ms=0, write512_over100ms=0, final=1))
        lines.append(row('worst', **common, role=2, start_us=110, end_us=112,
                         opcode=42, command_bytes=16384, request_bytes=13, actual=13, ok=1,
                         submitted=110, observed=111, resumed=112, seen=1, service_valid=1,
                         service_checks=1, service_samples=1, service_gap_max_us=1,
                         completion_gap_us=1, final=1))
        return content_fixture.TestSyncContent().fixture() + b''.join(lines)

    def test_complete_join_and_identical_duplicates(self):
        blob = self.fixture()
        result = sync_commands([blob, blob])
        self.assertEqual(result['status'], 'VALID')
        self.assertEqual(result['commands'], 1)
        self.assertEqual(result['write_bytes'], 16384)
        self.assertEqual(result['roles'][2]['worst']['opcode'], 42)
        self.assertIsNone(result['roles'][6]['worst'])

    def test_exact_schema_overflow_flags(self):
        changes = [(b'sync-command-snapshot: version=1', b'sync-command-snapshot: version=2'),
                   (b'identity=10 valid=1', b'identity=10 valid=0'),
                   (b'valid=1 active=0 invalid=0', b'valid=1 active=1 invalid=0'),
                   (b'valid=1 active=0 invalid=0', b'valid=1 active=0 invalid=1'),
                   (b'identity=10 valid=1', b'identity=18446744073709551616 valid=1'),
                   (b'role=6 reads=0', b'role=7 reads=0'),
                   (b'role=6 reads=0', b'role=6 reads=-1'),
                   (b'role=6 reads=0', 'role=6 reads=０'.encode()),
                   (b'sync-command-role:', b'sync-command-bogus:')]
        for before, after in changes:
            with self.subTest(change=after), self.assertRaises(ValueError):
                sync_commands([self.fixture().replace(before, after)])
        for ending in (b' extra=1', b' final=1', b' '):
            bad = self.fixture().replace(b'identity=10 valid=1 active=0 invalid=0 final=1\n',
                                        b'identity=10 valid=1 active=0 invalid=0 final=1' + ending + b'\n')
            with self.assertRaises(ValueError):
                sync_commands([bad])

    def test_missing_conflicting_extra_and_bindings(self):
        for prefix in (b'sync-command-snapshot:', b'sync-command-role:', b'sync-command-worst:'):
            bad = b'\n'.join(line for line in self.fixture().splitlines() if not line.startswith(prefix))
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                sync_commands([bad])
        command_lines = [x for x in self.fixture().splitlines(keepends=True) if x.startswith(b'sync-command-')]
        for index in range(len(command_lines)):
            bad = self.fixture().replace(command_lines[index], command_lines[index].replace(b'identity=10', b'identity=11'))
            with self.subTest(index=index), self.assertRaises(ValueError):
                sync_commands([bad])
        for before, after in [(b'pid=7 id=100 identity=10 valid=', b'pid=8 id=100 identity=10 valid='),
                              (b'id=100 identity=10 valid=', b'id=99 identity=10 valid='),
                              (b'csw_us=2', b'csw_us=3')]:
            with self.subTest(conflict=after), self.assertRaises(ValueError):
                sync_commands([self.fixture(), self.fixture().replace(before, after)])
        extra = next(x for x in command_lines if x.startswith(b'sync-command-worst:')).replace(b'role=2', b'role=6')
        with self.assertRaises(ValueError):
            sync_commands([self.fixture() + extra])

    def test_counts_sizes_and_outcomes(self):
        changes = [(b'writes=1 write_bytes=16384', b'writes=1 write_bytes=16385'),
                   (b'writes=1 write_bytes=16384', b'writes=1 write_bytes=1'),
                   (b'writes=1 write_bytes=16384', b'writes=0 write_bytes=16384'),
                   (b'failed=0 csws=1', b'failed=1 csws=1'),
                   (b'csws=1 csw_us=2', b'csws=2 csw_us=2'),
                   (b'csw_us=2 over100ms=0', b'csw_us=0 over100ms=0'),
                   (b'csw_us=2 over100ms=0', b'csw_us=2 over100ms=1'),
                   (b'csws=0 csw_us=0 over100ms=0', b'csws=0 csw_us=1 over100ms=0'),
                   (b'opcode=42 command_bytes=16384', b'opcode=256 command_bytes=16384'),
                   (b'opcode=42 command_bytes=16384', b'opcode=42 command_bytes=16385'),
                   (b'opcode=42 command_bytes=16384', b'opcode=42 command_bytes=512'),
                   (b'opcode=42 command_bytes=16384', b'opcode=40 command_bytes=16384'),
                   (b'opcode=42 command_bytes=16384', b'opcode=53 command_bytes=0'),
                   (b'opcode=42 command_bytes=16384', b'opcode=3 command_bytes=18')]
        for before, after in changes:
            with self.subTest(change=after), self.assertRaises(ValueError):
                sync_commands([self.fixture().replace(before, after)])


    def test_clocks_and_service_sample(self):
        changes = [(b'start_us=110 end_us=112', b'start_us=100 end_us=112'),
                   (b'start_us=110 end_us=112', b'start_us=110 end_us=131'),
                   (b'end_us=112 opcode=', b'end_us=109 opcode='),
                   (b'submitted=110 observed=111', b'submitted=109 observed=111'),
                   (b'observed=111 resumed=112', b'observed=113 resumed=112'),
                   (b'request_bytes=13 actual=13', b'request_bytes=12 actual=13'),
                   (b'actual=13 ok=1', b'actual=12 ok=1'),
                   (b'actual=13 ok=1', b'actual=13 ok=0'),
                   (b'seen=1 service_valid=1', b'seen=0 service_valid=1'),
                   (b'seen=1 service_valid=1', b'seen=1 service_valid=0'),
                   (b'service_checks=1 service_samples=1', b'service_checks=0 service_samples=1'),
                   (b'service_checks=1 service_samples=1', b'service_checks=257 service_samples=1'),
                   (b'service_gap_max_us=1 completion_gap_us=1', b'service_gap_max_us=0 completion_gap_us=1')]
        for before, after in changes:
            with self.subTest(change=after), self.assertRaises(ValueError):
                sync_commands([self.fixture().replace(before, after)])

    def test_class_variants_and_enclosing_bounds(self):
        for kind in ('read', 'flush', 'other'):
            blob = self.fixture()
            if kind == 'read':
                blob = blob.replace(b'role=2 reads=0 read_bytes=0 writes=1 write_bytes=16384',
                                    b'role=2 reads=1 read_bytes=16384 writes=0 write_bytes=0')
                blob = blob.replace(b'opcode=42 command_bytes=16384', b'opcode=40 command_bytes=16384')
            elif kind == 'flush':
                blob = blob.replace(b'writes=1 write_bytes=16384 flushes=0 other=0',
                                    b'writes=0 write_bytes=0 flushes=1 other=0')
                blob = blob.replace(b'opcode=42 command_bytes=16384', b'opcode=53 command_bytes=0')
            else:
                blob = blob.replace(b'writes=1 write_bytes=16384 flushes=0 other=0',
                                    b'writes=0 write_bytes=0 flushes=0 other=1')
                blob = blob.replace(b'opcode=42 command_bytes=16384', b'opcode=3 command_bytes=18')
            blob = blob.replace(b'write_csw_us=2', b'write_csw_us=0')
            with self.subTest(kind=kind):
                self.assertEqual(sync_commands([blob])['commands'], 1)
        blob = self.fixture(); content = sync_content([blob]); observed = sync_timing([blob])
        for key, value in [('callback_us', 1), ('calls', 0), ('max_us', 1), ('observe_max_us', 0), ('service_checks', 0)]:
            adapter = copy.deepcopy(observed)
            adapter['workers'][0]['phases']['files']['usb'][2][key] = value
            with self.subTest(bound=key), self.assertRaises(ValueError):
                sync_commands([blob], content, adapter)
        changed = copy.deepcopy(content); changed['snapshot_us'] = 999
        with self.assertRaises(ValueError):
            sync_commands([blob], changed, observed)

    def test_512_and_larger_write_subsets(self):
        blob = self.fixture()
        blob = change(blob, lambda l: l.startswith('sync-usb-timing:') and 'scope=files ' in l,
                      calls=2, callback_us=4, observed_calls=2, observe_us=2, resume_us=2,
                      service_checks=2, service_samples=2)
        blob = change(blob, lambda l: l.startswith('sync-command-role:') and 'role=2 ' in l,
                      writes=2, write_bytes=1536, csws=2, csw_us=4, write512_commands=1,
                      write_csw_us=4, write512_csw_us=2)
        blob = change(blob, lambda l: l.startswith('sync-command-worst:'), command_bytes=512)
        blob = change(blob, lambda l: l.startswith('sync-usb-command:') and 'scope=files ' in l, command_bytes=512)
        result = sync_commands([blob])
        self.assertEqual(result['write_bytes'], 1536)
        self.assertEqual(result['roles'][2]['summary']['write512_commands'], 1)
        for before, after in [(b'write512_commands=1', b'write512_commands=3'),
                              (b'write512_commands=1', b'write512_commands=0'),
                              (b'write512_csw_us=2', b'write512_csw_us=5'),
                              (b'write512_csw_us=2', b'write512_csw_us=0'),
                              (b'write_csw_us=4', b'write_csw_us=3'),
                              (b'write_over100ms=0', b'write_over100ms=1'),
                              (b'write512_over100ms=0', b'write512_over100ms=1'),
                              (b'write_bytes=1536', b'write_bytes=1024')]:
            with self.subTest(change=after), self.assertRaises(ValueError):
                sync_commands([blob.replace(before, after)])

    def test_slow_write_boundary(self):
        blob = self.fixture().replace(b'snapshot_us=1000', b'snapshot_us=1000000')
        blob = change(blob, lambda l: l.startswith('sync-record:'),
                      total_us=1000000, files_us=900000, devices_us=100000)
        blob = change(blob, lambda l: l.startswith('sync-worst:'), files_end_us=800000, end_us=900000)
        blob = change(blob, lambda l: l.startswith('sync-content-worst:'), end_us=700000)
        blob = change(blob, lambda l: l.startswith('sync-usb-command:') and 'scope=devices ' in l,
                      start_us=800001, end_us=800003, submitted=800001, observed=800002, resumed=800003)
        blob = change(blob, lambda l: l.startswith('sync-usb-timing:') and 'scope=files ' in l and 'phase=2 ' in l,
                      callback_us=200002, max_us=200002, over20ms=1, over100ms=1,
                      observe_us=200000, resume_us=1, observe_max_us=200000, resume_max_us=1,
                      service_checks=257, service_samples=2, service_gap_max_us=100000, completion_gap_max_us=50000)
        sample = dict(start_us=110, end_us=200112, opcode=42, command_bytes=512,
                      submitted=111, observed=200111, resumed=200112,
                      service_checks=257, service_samples=2, service_gap_max_us=100000, completion_gap_us=50000)
        blob = change(blob, lambda l: l.startswith('sync-usb-command:') and 'scope=files ' in l, **sample)
        blob = change(blob, lambda l: l.startswith('sync-command-worst:'), **sample)
        blob = change(blob, lambda l: l.startswith('sync-command-role:') and 'role=2 ' in l,
                      write_bytes=512, csw_us=200002, over100ms=1, write512_commands=1,
                      write_csw_us=200002, write512_csw_us=200002, write_over100ms=1, write512_over100ms=1)
        self.assertEqual(sync_commands([blob])['csw_us'], 200002)
        for before, after in [(b'over100ms=1 write512_commands=1', b'over100ms=0 write512_commands=1'),
                              (b'write512_over100ms=1', b'write512_over100ms=2'),
                              (b'write512_csw_us=200002', b'write512_csw_us=100000'),
                              (b'service_samples=2 service_gap_max_us', b'service_samples=1 service_gap_max_us')]:
            with self.subTest(change=after), self.assertRaises(ValueError):
                sync_commands([blob.replace(before, after)])

    def test_identical_legacy_maximum_and_outside_content(self):
        blob = self.fixture()
        # Values are otherwise valid, but the same callback cannot have two
        # different completion-gap observations in the two copied observers.
        bad = change(blob, lambda l: l.startswith('sync-command-worst:'), completion_gap_us=0)
        with self.assertRaises(ValueError):
            sync_commands([bad])
        observed = sync_timing([blob]); content = sync_content([blob])
        legacy = observed['workers'][0]['phases']['files']['usb'][2]
        legacy.update(calls=2, callback_us=5, max_us=3, observed_calls=2,
                      observe_us=3, resume_us=2, observe_max_us=2, resume_max_us=1,
                      service_checks=2, service_samples=2, service_gap_max_us=2, completion_gap_max_us=1)
        observed['workers'][0]['phases']['files']['command'].update(
            start_us=133, end_us=136, submitted=133, observed=135, resumed=136,
            service_gap_max_us=2)
        self.assertEqual(sync_commands([blob], content, observed)['commands'], 1)

    def test_explicit_no_work(self):
        blob = self.fixture(); observed = sync_timing([blob]); content = sync_content([blob])
        content.update(selection=dict(pid=0, id=0), worst=None, io=[])
        empty = row('snapshot', version=1, snapshot_us=1000, pid=0, id=0, identity=0,
                    valid=1, active=0, invalid=0, final=1)
        with self.assertRaises(ValueError):
            sync_commands([empty], content, observed)
        self.assertEqual(sync_commands([empty], content, observed, require_work=False)['commands'], 0)
        with self.assertRaises(ValueError):
            sync_commands([empty + self.fixture()], content, observed, require_work=False)


if __name__ == '__main__':
    unittest.main()
