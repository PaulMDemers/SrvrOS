"""Full synthetic RF parent plus unrenamed native-C priming child."""
import os
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

from test_wifi_channel_vco5_log import fixture as rf_fixture
from wifi_channel_vco5_log import HOOK, legacy_receipt
from wifi_trim5_log import plan
from wifi_cal5_log import parent
from wifi_prime5_log import check


def fixture():
    binary=Path(os.environ.get('WIFI_EARLY5_BINARY','build/tests/xhci-cal5-mcu-host')).resolve()
    child=subprocess.check_output([str(binary),'--trace-prime5'],text=True)
    rows=[s for s in child.splitlines() if s.startswith(('wifi-cal5','wifi-early5','wifi-mcu-loss-','wifi-prime5'))]
    i=next(i for i,s in enumerate(rows) if s.startswith('wifi-cal5-admission:'))
    rows[i]='wifi-cal5-admission: configured=1 parent=1 selected=1 vco=1 fields=1 context=1 owner=1'
    base=rf_fixture().replace('srvros.wifi-channel-vco5=1',
         'srvros.wifi-channel-vco5=1 srvros.wifi-cal5=1 srvros.wifi-cal5-cleanup=1 srvros.wifi-cal5-prime=1')
    after=legacy_receipt(HOOK,plan(12)[HOOK])
    return base.replace(after,'\n'.join(rows)+'\n'+after,1),rows


class Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.log,cls.rows=fixture()

    def test_actual_two_mac_lifetimes_one_calibration(self):
        r=check(self.log)
        self.assertTrue(r['prime5_complete'] and r['cal5_window_complete'])
        self.assertEqual(r['prime5_first_operations'],101)
        self.assertEqual(r['cal5_commands'],5)
        self.assertFalse(r['native_accepted'] or r['connectivity_tested'])

    def test_each_missing_duplicate_record(self):
        p=parent(self.log)
        with patch('wifi_cal5_log.parent',return_value=p):
            for row in self.rows:
                with self.subTest(missing=row),self.assertRaises(ValueError):
                    check(self.log.replace(row+'\n','',1))
                with self.subTest(duplicate=row),self.assertRaises(ValueError):
                    check(self.log.replace(row+'\n',row+'\n'+row+'\n',1))

    def test_transition_faults_and_reordered_stages(self):
        for a,b in (('protected=33','protected=32'),('context_preserved=1','context_preserved=0'),
                    ('resumed=1','resumed=0'),('new_pages=17','new_pages=16'),
                    ('calibration_once=1','calibration_once=0'),('held=1','held=0')):
            with self.subTest(a=a),self.assertRaises(ValueError):check(self.log.replace(a,b,1))
        with self.assertRaises(ValueError):check(self.log+'wifi-prime5-fault: version=1 reason=7 retained=1\n')
        with self.assertRaises(ValueError):check(self.log.replace('wifi-channel-vco5: status=0','wifi-channel-vco5: status=1'))


if __name__=='__main__':unittest.main()
