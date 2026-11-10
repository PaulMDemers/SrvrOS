import copy
import unittest

from sync_command_phases import sync_command_phases
from sync_commands import sync_commands
from sync_content import sync_content
from sync_timing import sync_timing
import test_sync_commands as command_fixture
from test_sync_commands import change


def row(kind, **values):
    return ('sync-bot-' + kind + ': ' + ' '.join('%s=%s' % item for item in values.items()) + '\n').encode()


def alter(blob, prefix, **updates):
    return change(blob, lambda line: line.startswith(prefix), **updates)


class TestSyncCommandPhases(unittest.TestCase):
    def fixture(self, byte_count=16384):
        blob = command_fixture.TestSyncCommands().fixture()
        blob = change(blob, lambda line: line.startswith('sync-usb-timing:') and
                      'scope=files ' in line and 'phase=1 ' in line,
                      calls=4, callback_us=8, observed_calls=4, observe_us=4,
                      resume_us=4, service_checks=4, service_samples=4)
        blob = change(blob, lambda line: line.startswith('sync-usb-command:') and
                      'scope=files ' in line, checkpoint_calls=4)
        calls = (byte_count + 4095) // 4096
        if byte_count != 16384:
            blob = change(blob, lambda line: line.startswith('sync-command-role:') and 'role=2 ' in line,
                          writes=int(bool(byte_count)), write_bytes=byte_count,
                          flushes=int(not byte_count), write512_commands=int(byte_count == 512),
                          write_csw_us=2 if byte_count else 0,
                          write512_csw_us=2 if byte_count == 512 else 0)
            opcode = 42 if byte_count else 53
            blob = change(blob, lambda line: line.startswith('sync-command-worst:'),
                          opcode=opcode, command_bytes=byte_count)
            blob = change(blob, lambda line: line.startswith('sync-usb-command:') and 'scope=files ' in line,
                          opcode=opcode, command_bytes=byte_count)
        common = dict(version=1, snapshot_us=1000, pid=7, id=100, identity=10,
                      role=2, csw_start_us=110, csw_end_us=112)
        blob += row('command', **common, valid=1, complete=1, ok=1, start_us=101,
                    end_us=113, checkpoint_calls=max(1, calls), checkpoint_us=1,
                    checkpoint_first_us=104 if calls > 1 else 112,
                    checkpoint_last_us=113, final=1)
        for phase, count, requested, first, last, clock in [
                (0, 1, 31, 101, 102, 1),
                (1, calls, byte_count, 103 if calls else 0, (104 if calls == 1 else 109) if calls else 0, calls),
                (2, 1, 13, 110, 112, 2)]:
            blob += row('phase', **common, phase=phase, calls=count,
                        requested=requested, actual=requested, us=clock,
                        first_us=first, last_us=last, final=1)
        blob += row('service', **common, empty_checks=0, event_checks=1,
                    last_empty_sample_us=0, final=1)
        return blob

    def test_complete_join_duplicates_and_historical_parent(self):
        blob = self.fixture()
        result = sync_command_phases([blob, blob])
        self.assertEqual(result['status'], 'VALID')
        self.assertEqual(result['span_us'], 12)
        self.assertEqual(result['phase_us'], 7)
        self.assertEqual(result['checkpoint_us'], 1)
        self.assertEqual(result['residual_us'], 4)
        self.assertEqual(result['phases'][1]['calls'], 4)
        # Existing v1 parser remains independent and ignores companion prefix.
        old = b'\n'.join(line for line in blob.splitlines() if not line.startswith(b'sync-bot-'))
        self.assertEqual(sync_commands([old]), sync_commands([blob]))
        with self.assertRaises(ValueError):
            sync_command_phases([old])

    def test_small_write_and_zero_data_flush(self):
        for size in (512, 4096, 8192, 16384, 0):
            with self.subTest(size=size):
                value = sync_command_phases([self.fixture(size)])
                self.assertEqual(value['phases'][1]['requested'], size)
                self.assertEqual(value['command']['checkpoint_calls'], max(1, (size + 4095) // 4096))
        empty = sync_command_phases([self.fixture(0)])['phases'][1]
        self.assertEqual([empty[k] for k in ('calls', 'requested', 'actual', 'us', 'first_us', 'last_us')], [0] * 6)

    def test_exact_schema_flags_and_unsigned_values(self):
        blob = self.fixture()
        changes = [(b'sync-bot-command: version=1', b'sync-bot-command: version=2'),
                   (b'valid=1 complete=1 ok=1', b'valid=0 complete=1 ok=1'),
                   (b'valid=1 complete=1 ok=1', b'valid=1 complete=0 ok=1'),
                   (b'valid=1 complete=1 ok=1', b'valid=1 complete=1 ok=0'),
                   (b'sync-bot-phase:', b'sync-bot-unrecognized:'),
                   (b'phase=2 calls=1 requested=13', b'phase=3 calls=1 requested=13'),
                   (b'last_empty_sample_us=0 final=1', b'last_empty_sample_us=-1 final=1'),
                   (b'last_empty_sample_us=0 final=1', 'last_empty_sample_us=０ final=1'.encode()),
                   (b'last_empty_sample_us=0 final=1', b'last_empty_sample_us=18446744073709551616 final=1')]
        for before, after in changes:
            with self.subTest(change=after), self.assertRaises(ValueError):
                sync_command_phases([blob.replace(before, after)])
        for suffix in (b' extra=1', b' final=1', b' '):
            with self.subTest(suffix=suffix), self.assertRaises(ValueError):
                sync_command_phases([blob.replace(b'last_empty_sample_us=0 final=1\n',
                                                   b'last_empty_sample_us=0 final=1' + suffix + b'\n')])

    def test_missing_conflicting_and_exact_identity(self):
        blob = self.fixture()
        for prefix in (b'sync-bot-command:', b'sync-bot-phase:', b'sync-bot-service:'):
            bad = b'\n'.join(line for line in blob.splitlines() if not line.startswith(prefix))
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                sync_command_phases([bad])
        details = [line for line in blob.splitlines(keepends=True) if line.startswith(b'sync-bot-')]
        for line in details:
            for before, after in [(b'pid=7 ', b'pid=8 '), (b'id=100 ', b'id=99 '),
                                  (b'identity=10 ', b'identity=11 '), (b'role=2 ', b'role=3 '),
                                  (b'csw_start_us=110 ', b'csw_start_us=109 '),
                                  (b'csw_end_us=112 ', b'csw_end_us=113 ')]:
                with self.subTest(line=line[:25], field=before), self.assertRaises(ValueError):
                    sync_command_phases([blob.replace(line, line.replace(before, after))])
        with self.assertRaises(ValueError):
            sync_command_phases([blob, alter(blob, 'sync-bot-command:', checkpoint_us=2)])
        # Each phase is required, and an additional phase is not tolerated.
        with self.assertRaises(ValueError):
            sync_command_phases([blob + details[1].replace(b'phase=0', b'phase=3')])

    def test_phase_lengths_order_and_parent_bounds(self):
        blob = self.fixture()
        for phase in range(3):
            prefix = lambda line: line.startswith('sync-bot-phase:') and ('phase=%d ' % phase) in line
            for fields in ({'calls': 2}, {'requested': 1}, {'actual': 1},
                           {'first_us': 100}, {'last_us': 114}, {'us': 100}):
                with self.subTest(phase=phase, fields=fields), self.assertRaises(ValueError):
                    sync_command_phases([change(blob, prefix, **fields)])
        for fields in ({'start_us': 100}, {'end_us': 131}, {'start_us': 102},
                       {'checkpoint_calls': 3}, {'checkpoint_calls': 5},
                       {'checkpoint_first_us': 102}, {'checkpoint_first_us': 110},
                       {'checkpoint_last_us': 112}, {'checkpoint_us': 10}):
            with self.subTest(fields=fields), self.assertRaises(ValueError):
                sync_command_phases([alter(blob, 'sync-bot-command:', **fields)])
        # One callback's accumulated wall must equal its exact endpoint span.
        for phase in (0, 2):
            with self.subTest(single_phase=phase), self.assertRaises(ValueError):
                sync_command_phases([change(blob, lambda line: line.startswith('sync-bot-phase:') and ('phase=%d ' % phase) in line, us=0)])
        small = self.fixture(512)
        with self.assertRaises(ValueError):
            sync_command_phases([alter(small, 'sync-bot-command:', checkpoint_us=0)])
        with self.assertRaises(ValueError):
            sync_command_phases([change(small, lambda line: line.startswith('sync-bot-phase:') and 'phase=1 ' in line, us=0)])
        empty = self.fixture(0)
        for key in ('us', 'first_us', 'last_us'):
            with self.subTest(empty=key), self.assertRaises(ValueError):
                sync_command_phases([change(empty, lambda line: line.startswith('sync-bot-phase:') and 'phase=1 ' in line, **{key: 1})])
        # Parent adapters are already validated observations, as in other parsers.
        observed = sync_timing([blob])
        for phase, key, value in [(0, 'calls', 0), (1, 'callback_us', 3), (2, 'callback_us', 1)]:
            adapter = copy.deepcopy(observed)
            adapter['workers'][0]['phases']['files']['usb'][phase][key] = value
            with self.subTest(parent=(phase, key)), self.assertRaises(ValueError):
                sync_command_phases([blob], sync=adapter)
        adapter = copy.deepcopy(observed)
        adapter['workers'][0]['phases']['files']['command']['checkpoint_calls'] = 3
        with self.assertRaises(ValueError):
            sync_command_phases([blob], sync=adapter)

    def test_ring_classification_and_sample_clock(self):
        blob = self.fixture()
        for values in ({'event_checks': 0}, {'empty_checks': 1},
                       {'last_empty_sample_us': 110}, {'last_empty_sample_us': 112}):
            with self.subTest(values=values), self.assertRaises(ValueError):
                sync_command_phases([alter(blob, 'sync-bot-service:', **values)])
        # A sampled empty ring precedes the final matching event; no new clock is implied.
        blob = change(blob, lambda line: line.startswith('sync-usb-timing:') and 'scope=files ' in line and 'phase=2 ' in line,
                      service_checks=4)
        blob = change(blob, lambda line: line.startswith('sync-usb-command:') and 'scope=files ' in line,
                      service_checks=4)
        blob = alter(blob, 'sync-command-worst:', service_checks=4)
        blob = alter(blob, 'sync-bot-service:', empty_checks=3, event_checks=1, last_empty_sample_us=110)
        self.assertEqual(sync_command_phases([blob])['service']['empty_checks'], 3)
        for at in (109, 112):
            with self.subTest(at=at), self.assertRaises(ValueError):
                sync_command_phases([alter(blob, 'sync-bot-service:', last_empty_sample_us=at)])
        # Empty checks between existing sample points need not supply an empty timestamp.
        self.assertEqual(sync_command_phases([alter(blob, 'sync-bot-service:', last_empty_sample_us=0)])['status'], 'VALID')

    def test_maximum_command_and_parent_snapshot_binding(self):
        blob = self.fixture()
        joined = sync_commands([blob])
        adapter = copy.deepcopy(joined)
        other = copy.deepcopy(adapter['roles'][2]['worst'])
        other.update(role=4, start_us=115, end_us=118)
        adapter['roles'][4]['worst'] = other
        with self.assertRaises(ValueError):
            sync_command_phases([blob], commands=adapter)
        # A later equal maximum in another role leaves the first callback selected.
        other['end_us'] = 117
        self.assertEqual(sync_command_phases([blob], commands=adapter)['status'], 'VALID')
        # An earlier equal maximum proves that the selected companion is wrong.
        other.update(start_us=105, end_us=107)
        with self.assertRaises(ValueError):
            sync_command_phases([blob], commands=adapter)
        # Equal raw start/end clocks cannot resolve ordering between roles.
        other.update(start_us=110, end_us=112)
        self.assertEqual(sync_command_phases([blob], commands=adapter)['status'], 'VALID')
        adapter['snapshot_us'] = 999
        with self.assertRaises(ValueError):
            sync_command_phases([blob], commands=adapter)
        selected = sync_content([blob]); selected['worst']['end_us'] = 112
        with self.assertRaises(ValueError):
            sync_command_phases([blob], content=selected)

    def test_explicit_no_content(self):
        blob = self.fixture()
        observed = sync_timing([blob]); selected = sync_content([blob]); joined = sync_commands([blob])
        selected['worst'] = None
        selected['selection'].update(pid=0, id=0)
        joined.update(commands=0, roles=[])
        header = row('command', version=1, snapshot_us=1000, pid=0, id=0, identity=0,
                     role=0, csw_start_us=0, csw_end_us=0, valid=0, complete=0, ok=0,
                     start_us=0, end_us=0, checkpoint_calls=0, checkpoint_us=0,
                     checkpoint_first_us=0, checkpoint_last_us=0, final=1)
        self.assertEqual(sync_command_phases([header], joined, selected, observed, require_work=False)['status'], 'VALID')
        with self.assertRaises(ValueError):
            sync_command_phases([header], joined, selected, observed)
        with self.assertRaises(ValueError):
            sync_command_phases([header.replace(b'complete=0', b'complete=1')], joined, selected, observed, require_work=False)


if __name__ == '__main__':
    unittest.main()
