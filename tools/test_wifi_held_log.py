import unittest
from unittest.mock import patch
from wifi_held_log import check,prior,plan,receipt,PROGRESS,summary
from test_wifi_bbp5_log import fixture as parent_fixture


def fixture(lna=12):
    return parent_fixture(lna).replace('srvros.wifi-bbp5-cycle=1','srvros.wifi-bbp5-cycle=1 srvros.wifi-held-channel=1')+PROGRESS+'\n'+'\n'.join(receipt(op,row) for op,row in enumerate(plan(lna)))+'\n'+summary(lna)+'\n'


class ProofTests(unittest.TestCase):
    def test_complete(self):
        self.assertEqual(check(fixture())['operations'],7785)
        self.assertTrue(check(fixture(0))['held_restored'])
    def test_each_receipt(self):
        good=fixture();parent=prior(good)
        with patch('wifi_held_log.prior',return_value=parent):
            for line in good.splitlines():
                if not line.startswith('wifi-held'):continue
                for replacement in ('',line+'\n'+line+'\n'):
                    with self.assertRaises(ValueError):check(good.replace(line+'\n',replacement))
            with self.assertRaises(ValueError):check(good+'wifi-held-unknown: ok=1\n')
    def test_fields_and_order(self):
        good=fixture();parent=prior(good)
        with patch('wifi_held_log.prior',return_value=parent):
            for a,b in [('phase=10','phase=9'),('local=294','local=293'),('selected=1','selected=0'),('restored=1','restored=0'),('operations=3732','operations=3731'),('write=1','write=0'),('cc=13','cc=1'),('residue=1008','residue=0'),('value=0x00000000ffffffff','value=0x00000000fffffffe')]:
                with self.assertRaises(ValueError):check(good.replace(a,b))
            rows=good.splitlines();a=rows.index(next(l for l in rows if l.startswith('wifi-held-usb:') and 'phase=11' in l));rows[a],rows[a+1]=rows[a+1],rows[a]
            with self.assertRaises(ValueError):check('\n'.join(rows))
            moved=good.replace(PROGRESS+'\n','').replace('wifi-bbp5-progress:',PROGRESS+'\nwifi-bbp5-progress:',1)
            with self.assertRaises(ValueError):check(moved)
