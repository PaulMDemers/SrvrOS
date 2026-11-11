"""Regression for native periodic files and their recurring snapshot markers."""
import unittest

from hardware_boot import limine_config
from wifi_early5_log import transport

RELEASE = '0123456789abcdef'
VOLUME = '0e535900'
SESSION = 'session-00066005112a7445-00'


def fixture():
    config = limine_config(RELEASE, f'ram-usb-wifi-fw-v144-laptop-probe-{VOLUME}-auto-return')
    command = next(l.split(':', 1)[1].strip() for l in config.splitlines()
                   if l.strip().startswith('cmdline:'))
    prefix = ('cmdline: ' + command + '\nworkspace-log: directory=/work/srvros-work/Logs/' +
              SESSION + ' periodic_seconds=10\n')

    def snapshot(name, now):
        nonlocal prefix
        prefix += f'workspace-log: snapshot={name} monotonic_us={now} total_before={len(prefix.encode())} tail_limit=4194304\n'
        return prefix.encode()

    boot = snapshot('boot.log', 1000)
    snapshot('session-a.log', 10000000)
    b = snapshot('session-b.log', 20000000)
    a = snapshot('session-a.log', 30000000)
    exit = snapshot('exit.log', 40000000)
    return {'boot.log': boot, 'session-a.log': a, 'session-b.log': b, 'exit.log': exit}


def verify(raw):
    return transport(raw, RELEASE, volume=VOLUME, session=SESSION)


class EarlyTransportTests(unittest.TestCase):
    def test_native_four_file_shape_and_recurring_periodic_marker(self):
        result = verify(fixture())
        self.assertEqual(result['snapshot_marker_counts'],
                         {'boot.log': 1, 'session-a.log': 2, 'session-b.log': 1, 'exit.log': 1})
        self.assertFalse(result['native_accepted'] or result['connectivity_tested'])

    def test_core_only_and_one_periodic_file(self):
        raw = fixture()
        for optional in ((), ('session-a.log',), ('session-b.log',)):
            verify({n: raw[n] for n in ('boot.log', 'exit.log', *optional)})

    def test_missing_core_unknown_and_wrong_owner_files(self):
        raw = fixture()
        for name in ('boot.log', 'exit.log'):
            changed = dict(raw); changed.pop(name)
            with self.assertRaises(ValueError): verify(changed)
        for name in ('unknown.log', 'wifi-scan.log', '../exit.log'):
            changed = dict(raw); changed[name] = raw['exit.log']
            with self.assertRaises(ValueError): verify(changed)

    def test_actual_prefix_boundaries_are_checked(self):
        raw = fixture()
        for name in raw:
            for mutate in (lambda b: b[:-1], lambda b: b + b'junk\n',
                           lambda b: b.replace(b'total_before=', b'total_before=0', 1),
                           lambda b: b.replace(b'tail_limit=4194304', b'tail_limit=2097152')):
                changed = dict(raw); changed[name] = mutate(changed[name])
                with self.subTest(name=name), self.assertRaises(ValueError): verify(changed)

    def test_duplicate_core_or_periodic_marker_refuses(self):
        for name, data in fixture().items():
            changed = fixture(); changed[name] = data + data.splitlines()[-1] + b'\n'
            with self.subTest(name=name), self.assertRaises(ValueError): verify(changed)

    def test_periodic_time_cannot_reverse(self):
        changed = fixture(); changed['session-a.log'] = changed['session-a.log'].replace(
            b'monotonic_us=30000000', b'monotonic_us=10000000')
        with self.assertRaises(ValueError): verify(changed)

    def test_stage_and_session_identity_are_exact(self):
        for old, new in ((RELEASE.encode(), b'ffffffffffffffff'),
                         (SESSION.encode(), b'session-00066005112a7446-00'),
                         (b'srvros.wifi-early5=1', b'srvros.wifi-early5=10')):
            changed = fixture(); changed['session-b.log'] = changed['session-b.log'].replace(old, new)
            with self.subTest(old=old), self.assertRaises(ValueError): verify(changed)

    def test_individually_valid_but_divergent_prefixes_refuse(self):
        changed = fixture()
        # B's own suffix, offsets and identity remain valid; its copy of an
        # earlier A timestamp disagrees with the shared boot/exit history.
        changed['session-b.log'] = changed['session-b.log'].replace(
            b'monotonic_us=10000000', b'monotonic_us=11000000')
        with self.assertRaises(ValueError): verify(changed)


if __name__ == '__main__':
    unittest.main()
