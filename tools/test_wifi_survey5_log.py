import os
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

from wifi_survey5_log import observe,check,TOKEN
from wifi_discovery5_log import check as selected_parent
from test_wifi_channel_vco5_log import fixture as rf_fixture
from wifi_channel_vco5_log import HOOK,legacy_receipt
from wifi_trim5_log import plan

BINARY=Path(os.environ.get('WIFI_SURVEY5_BINARY','build/tests/xhci-cal5-mcu-host')).resolve()

def emitted(mode=0):
    return TOKEN+'\n'+subprocess.check_output([str(BINARY),'--trace-survey5',str(mode)],text=True)

def full_parent(child):
    rows=[s for s in child.splitlines() if s.startswith(('wifi-cal5','wifi-early5','wifi-mcu-loss-',
         'wifi-prime5','wifi-survey5','wifi-discovery5'))]
    i=next(i for i,s in enumerate(rows) if s.startswith('wifi-cal5-admission:'))
    # The RF parent is explicitly modeled here, as in the existing priming
    # fixture. Native C separately composes the actual inverse engine/callback.
    rows[i]='wifi-cal5-admission: configured=1 parent=1 selected=1 vco=1 fields=1 context=1 owner=1'
    base=rf_fixture().replace('srvros.wifi-channel-vco5=1',
        'srvros.wifi-channel-vco5=1 srvros.wifi-cal5=1 srvros.wifi-cal5-cleanup=1 '
        'srvros.wifi-cal5-prime=1 srvros.wifi-survey5=1 srvros.wifi-discovery5=1')
    after=legacy_receipt(HOOK,plan(12)[HOOK])
    return base.replace(after,'\n'.join(rows)+'\n'+after,1)

class Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.log=emitted();cls.result=observe(cls.log)

    def test_actual_native_target_after_first_eight_and_full_parent(self):
        r=check(self.log)
        self.assertEqual(r['summary']['windows'],2)
        self.assertEqual(r['windows'][0]['choice']['status'],2)
        self.assertEqual(r['windows'][1]['choice']['status'],0)
        full=selected_parent(full_parent(self.log))
        self.assertTrue(full['demers5_selected'])
        self.assertEqual(full['survey5']['final_stop'],r['final_stop'])
        self.assertFalse(r['native_accepted'])

    def test_actual_native_capacity_deadline_and_security_failures(self):
        for mode,reason in ((1,2),(2,2),(3,1),(4,1),(11,3),(12,3),(14,3)):
            log=emitted(mode);r=observe(log)
            self.assertEqual(r['summary']['reason'],reason)
            self.assertFalse(r['summary']['selected'])
            self.assertTrue(r['cleanup_allowed'])
            with self.assertRaises(ValueError):check(log)

    def test_history_and_malformed_other_frames(self):
        for mode in (13,15):self.assertTrue(check(emitted(mode))['summary']['selected'])

    def test_missing_duplicate_and_unknown_records(self):
        rows=[s for s in self.log.splitlines() if s.startswith('wifi-survey5')]
        for row in rows:
            with self.subTest(missing=row),self.assertRaises((ValueError,IndexError)):
                observe(self.log.replace(row+'\n','',1))
            with self.subTest(duplicate=row),self.assertRaises((ValueError,IndexError)):
                observe(self.log.replace(row+'\n',row+'\n'+row+'\n',1))
        for changed in (self.log.replace(TOKEN,''),self.log+'wifi-survey5-unknown: value=1\n'):
            with self.assertRaises(ValueError):observe(changed)

    def test_mutated_deadline_context_stop_and_success(self):
        for old,new in (('budget_us=10000000','budget_us=20000000'),
                        ('maximum=16','maximum=17'),('retained_windows=1','retained_windows=0'),
                        ('inspected_after_stop=1','inspected_after_stop=0'),('windows=2','windows=1'),
                        ('begin=155','begin=154'),('end=209','end=210')):
            with self.subTest(old=old),self.assertRaises((ValueError,IndexError)):
                observe(self.log.replace(old,new,1))
        handoff=next(s for s in self.log.splitlines() if s.startswith('wifi-survey5-handoff:'))
        with self.assertRaises(ValueError):observe(self.log.replace(handoff,handoff.replace('context_preserved=1','context_preserved=0')))
        # Moving a context across its exact command or resetting only the
        # second clock must not pass the source-bound continuation ledger.
        clocks=[s for s in self.log.splitlines() if s.startswith('wifi-survey5-clock:')]
        bad=clocks[1].replace('deadline_us='+str(self.result['windows'][1]['clock']['deadline_us']),
                              'deadline_us='+str(self.result['windows'][1]['clock']['deadline_us']+1))
        with self.assertRaises(ValueError):observe(self.log.replace(clocks[1],bad))

if __name__=='__main__':unittest.main()
