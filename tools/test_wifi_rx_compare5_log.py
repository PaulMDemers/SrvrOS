"""Actual native-C child emissions composed with a modeled complete RF parent."""
import os
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

from test_wifi_early5_log import fixture as ledger_fixture
from wifi_cal5_log import parent
from wifi_rx_compare5_log import check, observe


def logging(arm):
    return f'wifi-rx-compare5-log: version=1 parked=1 periodic=0 before_mac={int(arm == "early")}\n'


def fixture(arm):
    log, rows = ledger_fixture(5 if arm == 'early' else 7)
    command, rest = log.split('\n', 1)
    return command + ' srvros.wifi-early5-timing=' + arm + '\n' + logging(arm) + rest, rows


class CompareLedgerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.logs = {arm: fixture(arm) for arm in ('early', 'late')}

    def test_both_actual_native_children_and_full_inverse_parent(self):
        for arm, (log, _) in self.logs.items():
            with self.subTest(arm=arm):
                r = check(log, arm=arm)
                self.assertEqual(r['operations'], 15719)
                self.assertTrue(r['logger_parked'] and r['early5_complete'])
                self.assertFalse(r['native_accepted'] or r['root_cause_proven'] or r['connectivity_tested'])

    def test_actual_native_timeout_is_observation_only(self):
        binary = Path(os.environ.get('WIFI_EARLY5_BINARY', 'build/tests/xhci-cal5-mcu-host')).resolve()
        for arm, mode in (('early', 6), ('late', 8)):
            log = ('cmdline: srvros.wifi-early5-timing=' + arm + '\n' + logging(arm) +
                   subprocess.check_output([str(binary), '--trace-early5', str(mode)], text=True))
            r = observe(log, arm=arm)
            self.assertFalse(r['summary']['complete'])
            self.assertEqual(r['summary']['commands'], 1)
            self.assertEqual(r['summary']['posted'], 32 if arm == 'early' else 0)
            self.assertFalse(r['summary']['stopped'])
            with self.assertRaises(ValueError):
                check(log, arm=arm)

    def test_logger_and_selected_arm_are_required(self):
        for arm, (log, _) in self.logs.items():
            for bad in (log.replace(logging(arm), ''),
                        log.replace('parked=1', 'parked=0'),
                        log.replace('periodic=0', 'periodic=1'),
                        log.replace(logging(arm), logging(arm) + 'workspace-log: snapshot=session-a.log invalid\n'),
                        log + logging(arm), log.replace('timing=' + arm, 'timing=unknown'),
                        log.replace('capacity=32', 'capacity=31')):
                with self.subTest(arm=arm), self.assertRaises(ValueError):
                    observe(bad, arm=arm)
            with self.assertRaises(ValueError):
                observe(log, arm='late' if arm == 'early' else 'early')

    def test_publication_must_match_actual_calibration_checkpoint(self):
        for arm, (log, _) in self.logs.items():
            line = next(l + '\n' for l in log.splitlines() if l.startswith('wifi-early5-publish:'))
            modified = log.replace(line, '')
            anchor = next(l + '\n' for l in modified.splitlines() if l.startswith(
                'wifi-cal5-calibration:' if arm == 'early' else 'wifi-cal5-admission:'))
            modified = modified.replace(anchor, anchor + line)
            with self.subTest(arm=arm), self.assertRaises(ValueError):
                observe(modified, arm=arm)

    def test_full_ledger_missing_duplicate_and_parent_faults(self):
        for arm, (log, rows) in self.logs.items():
            original = parent(log)
            with patch('wifi_cal5_log.parent', return_value=original):
                for row in rows:
                    for bad in (log.replace(row + '\n', '', 1), log.replace(row + '\n', row + '\n' + row + '\n', 1)):
                        with self.subTest(arm=arm, row=row), self.assertRaises(ValueError):
                            check(bad, arm=arm)
            with self.assertRaises(ValueError):
                check(log.replace('wifi-channel-vco5: status=0', 'wifi-channel-vco5: status=1'), arm=arm)


if __name__ == '__main__':
    unittest.main()
