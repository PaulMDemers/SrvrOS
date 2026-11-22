import unittest
from wifi_5ghz_prereq_log import TARGETS,PROGRESS,SUMMARY,check


def fixture():
    lines=['cmdline: srvros.wifi-5ghz-prereq=1',PROGRESS,'wifi-rf-vga-value: index=34 address=0x000000008007004a expected=0 observed=0']
    values=[17,34,51,68,85,0x44,0x45]
    for op in range(49):
        i,n=divmod(op,7)
        if i>=5 and n==3:
            h=0x4a000008|((i-4)<<16)
            lines.append(f'wifi-5ghz-prereq-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{TARGETS[i]:016x} value=0x{values[i]:016x} t=0x{h:016x} writes=0')
        else:
            a=TARGETS[i] if n==3 else (0x1004,0x2000,0x208)[n if n<3 else n-4]
            v=values[i] if n==3 else (0,0xf000f200,0x40000050)[n if n<3 else n-4]
            lines.append(f'wifi-5ghz-prereq-usb: op={op} address=0x{a:016x} value=0x{v:016x} ok=1 writes=0')
    lines.append(SUMMARY)
    lines.extend(f'wifi-5ghz-prereq-value: index={i} address=0x{a:016x} value=0x{v:016x}' for i,(a,v) in enumerate(zip(TARGETS,values)))
    return '\n'.join(lines)+'\n'


class ProofTests(unittest.TestCase):
    def test_valid(self):
        self.assertEqual(check(fixture())['values'],[17,34,51,68,85,68,69])

    def test_receipt_mutations(self):
        good=fixture()
        for line in good.splitlines()[1:]:
            for bad in (good.replace(line+'\n',''),good.replace(line+'\n',(line+'\n')*2)):
                with self.assertRaises(ValueError):check(bad)
        for old,new in [('srvros.wifi-5ghz-prereq=1','absent'),('submitted=49','submitted=50'),('writes=0','writes=1'),('cc=13','cc=1'),('residue=1008','residue=0'),('0x0000000000001004','0x0000000000001005'),('ok=1','ok=0'),('0x0000000040000050','0x0000000040000051')]:
            with self.assertRaises(ValueError):check(good.replace(old,new))
        with self.assertRaises(ValueError):check(good+'wifi-rf-g20: complete=1\n')
