import unittest
from unittest.mock import patch
from wifi_pa5_log import check,plan,command,PROGRESS,SUMMARY,prior
from test_wifi_frontend_log import fixture as prior_fixture


def fixture(overrides=None):
    originals={a:b for a,b,_ in plan()};originals.update(overrides or {})
    log=prior_fixture(overrides=originals).replace('srvros.wifi-frontend=1','srvros.wifi-frontend=1 srvros.wifi-pa5-cycle=1');rows=[PROGRESS]
    for op in range(490):
        q,n=divmod(op,7);a,v,w=command(q)
        if n!=3:
            j=n if n<3 else n-4;a=(0x1004,0x2000,0x208)[j];v=(0,0xf000f200,0x40000050)[j];w=False
        if n==3 and a&0x80000000:
            h=0x4a000008|((q%15+1)<<16);rows.append(f'wifi-pa5-command: op={op} write={int(w)} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x}')
        else:rows.append(f'wifi-pa5-usb: op={op} write={int(w)} address=0x{a:016x} value=0x{v:016x} ok=1')
    return log+'\n'.join(rows+[SUMMARY])+'\n'


class ProofTests(unittest.TestCase):
    def test_complete(self):
        r=check(fixture());self.assertEqual((r['operations'],r['pa5_words_restored']),(3305,14))
    def test_parent_tuple(self):
        for a,b,_ in plan():
            with self.assertRaises(ValueError):check(fixture({a:b^1}))
    def test_every_receipt(self):
        good=fixture()
        # Parent parser has its own exhaustive regressions; exercise each new
        # receipt without reparsing the identical half-megabyte parent1000times.
        with patch('wifi_pa5_log.prior',return_value=prior(good)):
            for line in good.splitlines():
                if not line.startswith('wifi-pa5'):continue
                with self.assertRaises(ValueError):check(good.replace(line+'\n',''))
                with self.assertRaises(ValueError):check(good.replace(line+'\n',(line+'\n')*2))
            with self.assertRaises(ValueError):check(good+'wifi-pa5-unknown: ok=1\n')
    def test_fields(self):
        good=fixture()
        for old,new in [('restored=1','restored=0'),('reads=429','reads=430'),('write=1','write=0'),('received=16','received=15'),('value=0x0000000089040000','value=0x0000000089640000')]:
            with self.assertRaises(ValueError):check(good.replace(old,new))
