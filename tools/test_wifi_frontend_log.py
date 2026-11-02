import unittest
from wifi_frontend_log import check,targets,PROGRESS,BOARD,SUMMARY,RAW
from test_wifi_a20_cycle_log import fixture as prior_fixture


def fixture(raw=False):
    log=prior_fixture().replace('srvros.wifi-a20-cycle=1','srvros.wifi-a20-cycle=1 srvros.wifi-frontend=1');rows=[BOARD,PROGRESS];values=[]
    for i,a in enumerate(targets()):
        v=((0x10000000+i*0x1001) if i<30 else (i*7)&255)
        if raw and a in RAW:v=0xffffffff
        for n in range(7):
            op=i*7+n
            if n!=3:
                j=n if n<3 else n-4;address=(0x1004,0x2000,0x208)[j];g=(0,0xf000f200,0x40000050)[j]
                rows.append(f'wifi-frontend-usb: op={op} address=0x{address:016x} value=0x{g:016x} ok=1 writes=0')
            elif i<30:rows.append(f'wifi-frontend-usb: op={op} address=0x{a:016x} value=0x{v:016x} ok=1 writes=0')
            else:
                h=0x4a000008|((i-29)<<16);rows.append(f'wifi-frontend-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x} writes=0')
        values.append(f'wifi-frontend-value: index={i} address=0x{a:016x} value=0x{v:016x}')
    return log+'\n'.join(rows+values+[SUMMARY])+'\n'


class ProofTests(unittest.TestCase):
    def test_complete(self):self.assertEqual(check(fixture())['frontend_operations'],308)
    def test_raw_slots(self):self.assertEqual(check(fixture(True))['frontend_values'].count(0xffffffff),4)
    def test_mutations(self):
        good=fixture()
        for line in good.splitlines():
            if not line.startswith('wifi-frontend'):continue
            with self.assertRaises(ValueError):check(good.replace(line+'\n',''))
            with self.assertRaises(ValueError):check(good.replace(line+'\n',(line+'\n')*2))
        with self.assertRaises(ValueError):check(good+'wifi-frontend-unknown: ok=1\n')
        for old,new in [('external5g=1','external5g=0'),('nic0=0x000000000000fd11','nic0=0x000000000000ff11'),('value=0x0000000010000000','value=0x00000000ffffffff'),('writes=0','writes=1')]:
            with self.assertRaises(ValueError):check(good.replace(old,new))
