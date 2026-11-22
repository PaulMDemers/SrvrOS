import unittest
from wifi_a20_inventory_log import check,targets,PROGRESS,SUMMARY
from test_wifi_5ghz_channel_log import fixture as channel_fixture


def fixture():
    log=channel_fixture().replace('srvros.wifi-5ghz-channel=1','srvros.wifi-5ghz-channel=1 srvros.wifi-a20-inventory=1');rows=[PROGRESS];values=[]
    for q,a in enumerate(targets()):
        v=q*7;h=0x4a000008|(((q+2)%15+1)<<16)
        for n in range(7):
            op=q*7+n
            if n==3:rows.append(f'wifi-a20-inventory-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x} writes=0')
            else:
                j=n if n<3 else n-4;g=(0,0xf000f200,0x40000050)[j];address=(0x1004,0x2000,0x208)[j]
                rows.append(f'wifi-a20-inventory-usb: op={op} address=0x{address:016x} value=0x{g:016x} ok=1 writes=0')
        values.append(f'wifi-a20-inventory-value: index={q} address=0x{a:016x} value=0x{v:016x}')
    return log+'\n'.join(rows+values+[SUMMARY])+'\n'


class ProofTests(unittest.TestCase):
    def test_complete(self):self.assertEqual(check(fixture())['a20_inventory_values'],list(range(0,168,7)))
    def test_mutations(self):
        log=fixture()
        for line in log.splitlines():
            if not line.startswith('wifi-a20-inventory'):continue
            with self.assertRaises(ValueError):check(log.replace(line+'\n',''))
            with self.assertRaises(ValueError):check(log.replace(line+'\n',(line+'\n')*2))
        with self.assertRaises(ValueError):check(log+'wifi-a20-inventory-unknown: value=0\n')
        with self.assertRaises(ValueError):check(log.replace('writes=0','writes=1'))
