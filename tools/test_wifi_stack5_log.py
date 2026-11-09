"""v137 checker regressions; complete-owner successes require actual C traces.

WIFI_STACK5_TRACE is the independently executed actual bounded-native C fixture
output; WIFI_STACK5_PARENT is its unchanged full RF/join parent for composition.
Optional WIFI_STACK5_RETRY_TRACE and WIFI_STACK5_FAILED_TRACE add actual retry
and failed-owner coverage. Composition is synthetic, never native acceptance.
"""
import os
from pathlib import Path
import re
import unittest
from unittest.mock import patch

import hardware_boot as boot
import test_wifi_stream5_log as old
import wifi_stack5_log as c
import wifi_stream5_log as legacy
import wifi_stream5_dhcp_log as legacy_retry

RELEASE = old.RELEASE


def boot_rows():
    tokens = ' '.join(sorted(c.BOOT_FIXED | {
        'srvros.desktop-release=' + RELEASE, 'srvros.workspace-volume=12345678'}))
    return ['cmdline: ' + tokens, 'laptop-capture: boot options=' + tokens]


def child_rows(fragment):
    return [s for s in fragment.splitlines() if s.startswith(('wifi-stream5', 'wifi-stack5'))]


def minimal_parent(fragment):
    """Modeled parent for isolated raw-owner mutations, never full acceptance."""
    child = child_rows(fragment)
    plan = c.fields(next(s for s in child if s.startswith('wifi-stream5-plan:')), 'wifi-stream5-plan')
    previous = {}
    for d in (0, 1):
        row = c.fields(next(s for s in child if s.startswith(f'wifi-stream5-context: p=0 d={d} ')),
                       'wifi-stream5-context')
        previous[d] = [row['w' + str(i)] for i in range(8)]
    parent = [c.LOG_PARKED,
              old.emit('wifi-join5-plan', tx=0x500000, rx=0x600000,
                       old=previous[1][2] | previous[1][3] << 32, slot=plan['slot'], max_rx=48),
              old.emit('wifi-handoff5', old_ring=0x100000, new_ring=0x200000),
              old.emit('wifi-tx5', ring=0x700000)]
    for d in (0, 1):
        parent.append(old.emit('wifi-join5-context', phase=8, dci=8 + d,
                               **{'w' + str(i): v for i, v in enumerate(previous[d])}))
    boots = boot_rows()
    return boots[:1] + [c.BOOT] + boots[1:] + parent + ['wifi-join5: modeled=1', child[0],
                                  'wifi-channel-vco5: modeled=1'] + child[1:] + [
        'wifi-firmware-capture: completed=yes; RF and connectivity NOT_TESTED',
        'laptop-capture: completed=yes']


def compose(reference, fragment):
    """Actual C child + unchanged parent; no fabricated success receipts."""
    foreign = ('wifi-nop5', 'wifi-wpa5', 'wifi-data5', 'wifi-retire5',
               'wifi-provider5', 'wifi-reuse5', 'wifi-stream5', 'wifi-stack5')
    lines = [s for s in reference.splitlines() if not s.startswith(foreign)]
    fresh = iter(boot_rows())
    lines = [next(fresh) if s.startswith(('cmdline: ', 'laptop-capture: boot options='))
             else s for s in lines]
    i = next(i for i, s in enumerate(lines) if s.startswith('laptop-capture: boot options='))
    lines[i:i + 1] = [c.BOOT, lines[i], c.LOG_PARKED]
    child = child_rows(fragment)
    if not child or child[0] != c.PROGRESS:
        raise ValueError('missing actual production child progress')
    i = next(i for i, s in enumerate(lines) if s.startswith('wifi-join5:'))
    # New sources emit both deferred parent observations before child entry.
    # Historical parents without them keep their original composition order.
    while i + 1 < len(lines) and lines[i + 1].startswith('wifi-join5-response:'):
        i += 1
    lines.insert(i + 1, child[0])
    i = next(i for i, s in enumerate(lines) if s.startswith('wifi-firmware-capture:'))
    lines[i:i] = child[1:]
    text = '\n'.join(lines) + '\n'
    marker = 'workspace-log: snapshot=exit.log '
    if marker in text:
        i = text.rindex(marker)
        text = text[:i] + re.sub(r'total_before=\d+', f'total_before={len(text[:i].encode())}', text[i:])
    return text


class Tests(unittest.TestCase):
    def actual(self, variable='WIFI_STACK5_TRACE'):
        value = os.environ.get(variable)
        if not value:
            self.skipTest(variable + ' actual C fixture not supplied')
        return Path(value).read_text()

    def check_owner(self, rows):
        text = '\n'.join(rows)
        with patch.object(c, 'parent', return_value={'eapol5_complete': True}) as parent:
            result = c.check(text, RELEASE)
            parent.assert_called_once_with(text, stream=True)
            return result

    def test_exact_boot_identity(self):
        rows = boot_rows()
        self.assertEqual(c.boot_identity('\n'.join(rows)), RELEASE)
        for token in c.BOOT_FIXED:
            with self.subTest(token=token), self.assertRaises(ValueError):
                c.boot_identity('\n'.join(s.replace(token, '') for s in rows))
        bad = [rows[:1], rows + rows[:1], [s + ' srvros.wifi-stack5=1' for s in rows],
               [s + ' srvros.wifi-stream5-dhcp=1' for s in rows],
               [s + ' srvros.wifi-reuse5=1' for s in rows],
               [rows[0], rows[1] + ' srvros.trackpad=gestures']]
        for value in bad:
            with self.assertRaises(ValueError):
                c.boot_identity('\n'.join(value))
        with self.assertRaises(ValueError):
            c.boot_identity('\n'.join(rows), 'f' * 16)

    def test_cleanup_recipe_is_explicit_and_legacy_modes_refuse_it(self):
        config=boot.limine_config(RELEASE,'ram-usb-wifi-fw-v138-laptop-probe-12345678-auto-return')
        tokens=next(s.strip()[9:] for s in config.splitlines() if s.strip().startswith('cmdline: '))
        rows=['cmdline: '+tokens,'laptop-capture: boot options='+tokens]
        self.assertEqual(c.boot_identity('\n'.join(rows)),RELEASE)
        for checker in (legacy,legacy_retry):
            with self.assertRaises(ValueError):checker.boot_identity('\n'.join(rows))
        with self.assertRaises(ValueError):c.boot_identity('\n'.join(rows)+'\n'+rows[0])
        with self.assertRaises(ValueError):c.boot_identity('\n'.join(rows).replace('srvros.wifi-cal5-cleanup=1','srvros.wifi-cal5-cleanup=2'))

    def test_matched_stage_contract(self):
        config = boot.limine_config(RELEASE, 'ram-usb-wifi-fw-v137-laptop-probe-12345678-auto-return')
        actual = next(s.strip()[9:].split() for s in config.splitlines()
                      if s.strip().startswith('cmdline: '))
        self.assertEqual(set(actual), set(boot_rows()[0][9:].split()))

    def test_old_modes_remain_strict(self):
        for checker in (legacy, legacy_retry):
            with self.assertRaises(ValueError):
                checker.boot_identity('\n'.join(boot_rows()))
        for rows in (old.boot_rows(), [s.replace('srvros.wifi-stack5=1', '') for s in boot_rows()]):
            with self.assertRaises(ValueError):
                c.boot_identity('\n'.join(rows))

    def test_actual_owner_success_and_scope(self):
        result = self.check_owner(minimal_parent(self.actual()))
        self.assertTrue(result['stack5_complete'] and result['stack5_ordinary_rx'])
        self.assertEqual(result['stack5_exact_pings'], 64)
        self.assertEqual(result['stack5_exact_body_bytes'], 2048)
        self.assertFalse(result['stack5_tcp'] or result['stack5_http'] or result['stack5_backend_retired'])

    def test_actual_retry_and_failure_if_supplied(self):
        retry = self.actual('WIFI_STACK5_RETRY_TRACE')
        self.assertEqual(self.check_owner(minimal_parent(retry))['stack5_discover_retries'], 1)
        failure = os.environ.get('WIFI_STACK5_FAILED_TRACE')
        if failure:
            with self.assertRaises(ValueError):
                self.check_owner(minimal_parent(Path(failure).read_text()))

    def test_missing_duplicate_order_and_columns(self):
        rows = minimal_parent(self.actual())
        indices = [i for i, s in enumerate(rows) if s.startswith(('wifi-stream5', 'wifi-stack5'))]
        for i in indices:
            for changed in (rows[:i] + rows[i + 1:], rows[:i] + [rows[i]] + rows[i:]):
                with self.subTest(row=i), self.assertRaises(ValueError):
                    self.check_owner(changed)
            changed = rows.copy()
            changed[i] += ' 0'
            with self.assertRaises(ValueError):
                self.check_owner(changed)
        for a, b in zip(indices, indices[1:]):
            changed = rows.copy()
            changed[a], changed[b] = changed[b], changed[a]
            with self.subTest(a=a, b=b), self.assertRaises(ValueError):
                self.check_owner(changed)

    def test_production_and_private_probe_mutations(self):
        rows = minimal_parent(self.actual())
        cases = [('wifi-stream5-probe:', 'complete', 1), ('wifi-stream5-keys:', 'installs', 2),
                 ('wifi-stack5:', 'completed', 0), ('wifi-stack5:', 'dropped', 1),
                 ('wifi-stack5:', 'retired', 0), ('wifi-stack5:', 'backend_retired', 1),
                 ('wifi-stack5:', 'elapsed_us', 15000000),
                 ('wifi-stack5-final:', 'detached', 0), ('wifi-stack5-final:', 'provider_retired', 0),
                 ('wifi-stack5-workload:', 'version', 2), ('wifi-stack5-workload:', 'complete', 0),
                 ('wifi-stack5-workload:', 'phase', 4), ('wifi-stack5-workload:', 'reason', 1),
                 ('wifi-stack5-workload:', 'dhcp', 0), ('wifi-stack5-workload:', 'retries', 2),
                 ('wifi-stack5-workload:', 'dns', 0), ('wifi-stack5-workload:', 'matched', 63),
                 ('wifi-stack5-workload:', 'icmp_bytes', 39),
                 ('wifi-stack5-workload:', 'payload_bytes', 31),
                 ('wifi-stack5-workload:', 'exact', 63), ('wifi-stack5-workload:', 'body_bytes', 2047),
                 ('wifi-stack5-workload:', 'setup_tx', 12),
                 ('wifi-stack5-workload:', 'polls', 150001),
                 ('wifi-stack5-workload:', 'services', 150000),
                 ('wifi-stack5-workload:', 'private_erased', 0)]
        for prefix, key, value in cases:
            i = next(i for i, s in enumerate(rows) if s.startswith(prefix))
            changed = rows.copy()
            changed[i] = re.sub(r'\b' + key + r'=\d+', key + '=' + str(value), changed[i])
            with self.subTest(prefix=prefix, key=key), self.assertRaises(ValueError):
                self.check_owner(changed)
        i = next(i for i, s in enumerate(rows) if s.startswith('wifi-stack5-pings: '))
        values = rows[i].split(': ', 1)[1].split(' ')
        for column in range(64):
            for value in ('0', values[max(0, column - 1)] if column else values[1], '18446744073709551616'):
                changed = rows.copy()
                v = values.copy()
                v[column] = value
                changed[i] = 'wifi-stack5-pings: ' + ' '.join(v)
                with self.subTest(column=column, value=value), self.assertRaises(ValueError):
                    self.check_owner(changed)
            changed = rows.copy()
            changed[i] = 'wifi-stack5-pings: ' + ' '.join(values[:column] + values[column + 1:])
            with self.subTest(missing_column=column), self.assertRaises(ValueError):
                self.check_owner(changed)
        for extra in ('wifi-stream5-dhcp: 0', 'wifi-stack5-http: complete=1', 'wifi-wpa5: complete=1'):
            with self.assertRaises(ValueError):
                self.check_owner(rows + [extra])

    def test_original_auth_observations(self):
        rows = minimal_parent(self.actual())
        i = next(i for i, s in enumerate(rows) if s.startswith('wifi-stack5-auth: '))
        cases = dict(attempted=0, auth_sent=0, assoc_sent=0, sends=4, polls=80001,
                     ignored=80001, status=1, aid=2008, present=0, peer=2, ok=0,
                     invalid=1, rejected=1, clocks=20002, elapsed_us=750000, rollback=1)
        for key, value in cases.items():
            changed = rows.copy()
            changed[i] = re.sub(r'\b' + key + r'=\d+', key + '=' + str(value), rows[i])
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.check_owner(changed)
        for key, value in [('polls', 0), ('aid', 0), ('clocks', 1), ('sends', 6),
                           ('elapsed_us', 18446744073709551616)]:
            changed = rows.copy()
            changed[i] = re.sub(r'\b' + key + r'=\d+', key + '=' + str(value), rows[i])
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                self.check_owner(changed)
        # Four successful response phases require four nonignored polls;
        # AUTH's initial clock plus its polls leaves at least three polls for
        # ASSOC/M1/M3. These pairs were impossible despite individual bounds.
        for key, value in [('ignored', 1), ('clocks', 3)]:
            changed = rows.copy()
            changed[i] = re.sub(r'\bpolls=\d+', 'polls=4', rows[i])
            changed[i] = re.sub(r'\b' + key + r'=\d+', key + '=' + str(value), changed[i])
            with self.subTest(impossible_counter=key), self.assertRaises(ValueError):
                self.check_owner(changed)
        for key, value in [('attempted', '01'), ('status', '-1'), ('peer', '0x0000000000000001')]:
            changed = rows.copy()
            changed[i] = re.sub(r'\b' + key + r'=\d+', key + '=' + value, rows[i])
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                self.check_owner(changed)
        # Unrelated AUTH-phase management traffic is allowed, and is included
        # in the original owner's ignored count. A matching malformed/rejected
        # peer response ends the exchange and cannot accompany success.
        changed = rows.copy()
        changed[i] = old.emit('wifi-stack5-auth', attempted=1, auth_sent=1, assoc_sent=1,
            sends=5, polls=8, ignored=1, status=0, aid=2007, present=2, peer=1,
            ok=1, invalid=0, rejected=0, clocks=3, elapsed_us=749999, rollback=0)
        self.assertTrue(self.check_owner(changed)['stack5_complete'])
        for key, value in [('ignored', 0), ('present', 3), ('clocks', 10)]:
            bad = changed.copy()
            bad[i] = re.sub(r'\b' + key + r'=\d+', key + '=' + str(value), changed[i])
            with self.subTest(consistency=key), self.assertRaises(ValueError):
                self.check_owner(bad)
        fields = changed[i].split(' ')
        fields[1], fields[2] = fields[2], fields[1]
        changed[i] = ' '.join(fields)
        with self.assertRaises(ValueError):
            self.check_owner(changed)

    def test_entire_unchanged_parent_if_supplied(self):
        parent_path = os.environ.get('WIFI_STACK5_PARENT')
        if not parent_path:
            self.skipTest('unchanged complete RF/join parent not supplied')
        text = compose(Path(parent_path).read_text(), self.actual())
        self.assertLessEqual(len(text.encode()), 2097152 - 32768,
                             'actual composed prefix consumes the explicit logger reserve')
        result = c.check(text, RELEASE)
        self.assertTrue(result['stack5_complete'] and result['eapol5_complete'] and
                        result['power5_originals_restored'])
        for prefix in ('wifi-eapol5:', 'wifi-join5:', 'wifi-cal5:', 'wifi-channel-vco5:',
                       'wifi-power5:', 'wifi-handoff5:', 'wifi-peer5:'):
            row = next(s for s in text.splitlines() if s.startswith(prefix))
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                c.check(text.replace(row + '\n', '', 1), RELEASE)


if __name__ == '__main__':
    unittest.main()
