import unittest
from unittest.mock import patch
from wifi_bbp5_log import check,plan,command,fields,PROGRESS,SUMMARY,prior
from test_wifi_pa5_log import fixture as prior_fixture


def fixture(lna=12,polls=1):
    log=prior_fixture({a:b for a,b,_ in plan()}).replace('srvros.wifi-pa5-cycle=1','srvros.wifi-pa5-cycle=1 srvros.wifi-bbp5-cycle=1');rows=[PROGRESS]+fields(polls,lna,0,0)
    for op in range(735):
        q,n=divmod(op,7);a,v,w=command(q,lna)
        if n!=3:j=n if n<3 else n-4;a=(0x1004,0x2000,0x208)[j];v=(0,0xf000f200,0x40000050)[j];w=False
        rows.append(f'wifi-bbp5-usb: op={op} write={int(w)} address=0x{a:016x} value=0x{v:016x} ok=1')
    return log+'\n'.join(rows+[SUMMARY])+'\n'


class ProofTests(unittest.TestCase):
    def test_complete(self):
        r=check(fixture());self.assertEqual((r['operations'],r['bbp5_words_restored'],r['gain5_agc8']),(4053,21,0x122c3cf2))
        self.assertEqual(check(fixture(0,3))['gain5_lna'],0)
    def test_receipts(self):
        good=fixture()
        with patch('wifi_bbp5_log.prior',return_value=prior(good)):
            for line in good.splitlines():
                if not line.startswith('wifi-bbp5'):continue
                with self.assertRaises(ValueError):check(good.replace(line+'\n',''))
                with self.assertRaises(ValueError):check(good.replace(line+'\n',(line+'\n')*2))
            with self.assertRaises(ValueError):check(good+'wifi-bbp5-unknown: ok=1\n')
    def test_fields(self):
        good=fixture()
        for a,b in [('lna=12','lna=13'),('logical40','logical30'),('write=1','write=0'),('phase=7 us=2','phase=7 us=1'),('reads=693','reads=694'),('raw_block_logged=0','raw_block_logged=1'),('value=0x00000000ffffffff','value=0x00000000fffffffe')]:
            with self.assertRaises(ValueError):check(good.replace(a,b))
