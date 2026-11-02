import unittest
from wifi_5ghz_channel_log import commands,check,PROGRESS,SYNTH_SUMMARY,BAND_SUMMARY,SUMMARY_CHANNEL
from wifi_5ghz_band_log import BAND_PROGRESS,SUMMARY
from test_wifi_5ghz_band_log import fixture as band_fixture


def fixture():
    log=band_fixture().replace('srvros.wifi-5ghz-band=1','srvros.wifi-5ghz-band=1 srvros.wifi-5ghz-channel=1').replace(BAND_PROGRESS,PROGRESS).replace(SUMMARY,BAND_SUMMARY)
    rows=['wifi-5ghz-band-phase: index=8 start=1']
    for q,(write,r,v) in enumerate(commands()):
        for n in range(7):
            op=q*7+n
            if n==3:
                h=0x4a000008|(((q+9)%15+1)<<16);a=0x80000000|r
                rows.append(f'wifi-5ghz-synth-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x} writes={int(write)}')
            else:
                j=n if n<3 else n-4;a=(0x1004,0x2000,0x208)[j];g=(0,0xf000f200,0x40000050)[j]
                rows.append(f'wifi-5ghz-synth-usb: op={op} address=0x{a:016x} value=0x{g:016x} ok=1 writes=0')
    log=log.replace('wifi-5ghz-band-phase: index=5 start=1','\n'.join(rows)+'\nwifi-5ghz-band-phase: index=5 start=1')
    log=log.replace(BAND_SUMMARY,SYNTH_SUMMARY+'\n'+BAND_SUMMARY)
    return log+SUMMARY_CHANNEL+'\n'


class ProofTests(unittest.TestCase):
    def test_complete(self):self.assertEqual(check(fixture())['operations'],1667)
    def test_removed_duplicated_receipts(self):
        good=fixture()
        for line in good.splitlines()[1:]:
            with self.assertRaises(ValueError):check(good.replace(line+'\n',''))
            with self.assertRaises(ValueError):check(good.replace(line+'\n',(line+'\n')*2))
    def test_mutations(self):
        good=fixture()
        for old,new in [('operations=1667','operations=1666'),('channel=161','channel=6'),('writes=34','writes=33'),('restored=1','restored=0'),('0x0000000000000068','0x0000000000000028'),('0x0000000040000050','0x0000000040000051'),('ok=1','ok=0'),('writes=1','writes=0')]:
            with self.assertRaises(ValueError):check(good.replace(old,new))
    def test_unknown_receipt(self):
        with self.assertRaises(ValueError):check(fixture()+'wifi-5ghz-synth-unknown: ok=1\n')

    def test_phase_scope(self):
        good=fixture();marker='wifi-5ghz-band-phase: index=8 start=1\n'
        with self.assertRaises(ValueError):check(good.replace(marker,'').replace('wifi-5ghz-band-phase: index=2 start=1',marker+'wifi-5ghz-band-phase: index=2 start=1'))
