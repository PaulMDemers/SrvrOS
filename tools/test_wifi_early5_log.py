"""Synthetic full RF parent plus emitted actual native-C calibration child.

Only the parent-admission scalars are supplied by the synthetic parent fixture;
this assembly is parser qualification and is never a hardware session.
"""
import os
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

from test_wifi_channel_vco5_log import fixture as rf_fixture
from wifi_channel_vco5_log import HOOK, legacy_receipt
from wifi_trim5_log import plan as trim_plan
from wifi_early5_log import check
from wifi_cal5_log import parent


def fixture():
    binary = Path(os.environ.get('WIFI_EARLY5_BINARY', 'build/tests/xhci-cal5-mcu-host')).resolve()
    child = subprocess.check_output([str(binary), '--trace-early5', '4'], text=True)
    lines = [l for l in child.splitlines() if l.startswith(('wifi-cal5', 'wifi-early5', 'wifi-mcu-loss-'))]
    admission = [i for i, l in enumerate(lines) if l.startswith('wifi-cal5-admission:')]
    assert len(admission) == 1
    lines[admission[0]] = 'wifi-cal5-admission: configured=1 parent=1 selected=1 vco=1 fields=1 context=1 owner=1'
    child = '\n'.join(lines) + '\n'
    base = rf_fixture().replace('srvros.wifi-channel-vco5=1',
                               'srvros.wifi-channel-vco5=1 srvros.wifi-cal5=1 srvros.wifi-early5=1')
    after = legacy_receipt(HOOK, trim_plan(12)[HOOK])
    return base.replace(after, child + after, 1), lines


class EarlyLedgerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.log, cls.rows = fixture()

    def test_native_child_complete(self):
        r = check(self.log)
        self.assertTrue(r['early5_complete'])
        self.assertFalse(r['early5_observation']['native_accepted'])
        self.assertEqual(r['operations'], 15719)

    def test_actual_child_missing_or_duplicate_rows(self):
        p = parent(self.log)
        with patch('wifi_cal5_log.parent', return_value=p):
            for row in self.rows:
                with self.subTest(missing=row), self.assertRaises(ValueError):
                    check(self.log.replace(row + '\n', '', 1))
                with self.subTest(duplicate=row), self.assertRaises(ValueError):
                    check(self.log.replace(row + '\n', row + '\n' + row + '\n', 1))

    def test_parent_restoration_required(self):
        with self.assertRaises(ValueError):
            check(self.log.replace('wifi-channel-vco5: status=0', 'wifi-channel-vco5: status=1'))


if __name__ == '__main__':
    unittest.main()
