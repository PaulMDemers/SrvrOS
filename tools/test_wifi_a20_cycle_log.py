import unittest
from wifi_a20_cycle_log import check,commands,PROGRESS,SUMMARY,BAND_SUMMARY,CHANNEL_SUMMARY
from wifi_5ghz_channel_log import BAND_SUMMARY as OLD_BAND,SUMMARY_CHANNEL as OLD_CHANNEL
from test_wifi_5ghz_channel_log import fixture as channel_fixture


def fixture():
    log=channel_fixture().replace('srvros.wifi-5ghz-channel=1','srvros.wifi-5ghz-channel=1 srvros.wifi-a20-cycle=1').replace(OLD_BAND,BAND_SUMMARY).replace(OLD_CHANNEL,CHANNEL_SUMMARY)
    rows=[PROGRESS,'wifi-5ghz-band-phase: index=9 start=1']
    for q,(write,a,v) in enumerate(commands()):
        h=0x4a000008|(((q+3)%15+1)<<16)
        for n in range(7):
            op=q*7+n
            if n==3:rows.append(f'wifi-a20-cycle-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x} writes={int(write)}')
            else:
                j=n if n<3 else n-4;address=(0x1004,0x2000,0x208)[j];g=(0,0xf000f200,0x40000050)[j]
                rows.append(f'wifi-a20-cycle-usb: op={op} address=0x{address:016x} value=0x{g:016x} ok=1 writes=0')
    needle='wifi-5ghz-synth-usb: op=623 '
    pos=log.index(needle);log=log[:pos]+'\n'.join(rows)+'\nwifi-5ghz-band-phase: index=8 start=1\n'+log[pos:]
    return log.replace(BAND_SUMMARY,SUMMARY+'\n'+BAND_SUMMARY)


class ProofTests(unittest.TestCase):
    def test_complete(self):self.assertEqual(check(fixture())['touched_words_restored'],41)
    def test_all_cycle_receipts(self):
        good=fixture()
        for line in good.splitlines():
            if not line.startswith('wifi-a20-cycle'):continue
            with self.assertRaises(ValueError):check(good.replace(line+'\n',''))
            with self.assertRaises(ValueError):check(good.replace(line+'\n',(line+'\n')*2))
        with self.assertRaises(ValueError):check(good+'wifi-a20-cycle-unknown: ok=1\n')
    def test_mutations(self):
        good=fixture()
        for old,new in [('operations=2507','operations=2506'),('writes=48','writes=47'),('0x000000000000008a','0x0000000000000000'),('restored=1','restored=0'),('writes=1','writes=0'),('ok=1','ok=0')]:
            with self.assertRaises(ValueError):check(good.replace(old,new))
        with self.assertRaises(ValueError):check(good.replace('index=9 start=1','index=8 start=1'))
