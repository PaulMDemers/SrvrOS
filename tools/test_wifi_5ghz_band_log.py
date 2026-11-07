from pathlib import Path
import unittest
from wifi_5ghz_band_log import PREFIXES,START,SELECTED,BAND_PROGRESS,SUMMARY,check
from wifi_5ghz_prereq_log import TARGETS,PROGRESS


def snapshot(prefix,values):
    rows=[]
    for op in range(49):
        i,n=divmod(op,7)
        if i>=5 and n==3:
            h=0x4a000008|((i-4)<<16);rows.append(f'{prefix}-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{TARGETS[i]:016x} value=0x{values[i]:016x} t=0x{h:016x} writes=0')
        else:
            j=n if n<3 else n-4;a=TARGETS[i] if n==3 else (0x1004,0x2000,0x208)[j];v=values[i] if n==3 else (0,0xf000f200,0x40000050)[j]
            rows.append(f'{prefix}-usb: op={op} address=0x{a:016x} value=0x{v:016x} ok=1 writes=0')
    tail=[f'{prefix}: status=0 complete=1 valid=1 operations=49 submitted=49 reads=47 exchanges=2 observed=7 writes=0 network_out=0']
    tail.extend(f'{prefix}-value: index={i} address=0x{a:016x} value=0x{v:016x}' for i,(a,v) in enumerate(zip(TARGETS,values)))
    return rows,tail


def fixture():
    rows=['cmdline: srvros.wifi-5ghz-prereq=1 srvros.wifi-5ghz-band=1',PROGRESS,'wifi-rf-vga-value: index=34 synthetic=1',BAND_PROGRESS];tail=[]
    blob=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-band.bin').read_bytes();pairs=list(zip(blob[::3],blob[1::3],blob[2::3]))
    for phase,prefix in enumerate(PREFIXES,1):
        rows.append(f'wifi-5ghz-band-phase: index={phase} start=1')
        if phase in (1,4,7):
            r,t=snapshot(prefix,SELECTED if phase==4 else START);rows.extend(r);tail.extend(t);continue
        if phase in (2,6):
            plan=pairs if phase==2 else [(6,0,0),(5,0,0)];base=0 if phase==2 else 5;count=len(plan)
            for op in range(count*14):
                q,n=divmod(op,7)
                if n==3:
                    b,r,v=plan[q%count];h=0x4a000008|(((base+q)%15+1)<<16);a=0x80000000|(b<<16)|r;rows.append(f'{prefix}-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x} writes={int(q<count)}')
                else:
                    j=n if n<3 else n-4;rows.append(f'{prefix}-usb: op={op} address=0x{(0x1004,0x2000,0x208)[j]:016x} value=0x{(0,0xf000f200,0x40000050)[j]:016x} ok=1 writes=0')
            count*=14
        else:
            addresses=(0x13c8,0x13a0) if phase==3 else (0x13a0,0x13c8);before=(0,0) if phase==3 else (0x01010102,5);after=(5,0x01010102) if phase==3 else (0,0);count=18
            for op in range(18):
                i,n=divmod(op,9)
                if n<3 or n>=6:j=n if n<3 else n-6;a=(0x1004,0x2000,0x208)[j];v=(0,0xf000f200,0x40000050)[j]
                else:a=addresses[i];v=before[i] if n==3 else after[i]
                rows.append(f'{prefix}-usb: op={op} address=0x{a:016x} value=0x{v:016x} ok=1 writes={int(n==4)}')
        tail.append(f'{prefix}: complete=1 valid=1 operations={count} submitted={count}')
    return '\n'.join(rows+tail+[SUMMARY])+'\n'


class ProofTests(unittest.TestCase):
    def test_complete(self):self.assertTrue(check(fixture())['restored'])
    def test_mutations(self):
        good=fixture()
        for line in good.splitlines()[1:]:
            with self.assertRaises(ValueError):check(good.replace(line+'\n',''))
            with self.assertRaises(ValueError):check(good.replace(line+'\n',(line+'\n')*2))
        for old,new in [('restored=1','restored=0'),('operations=771','operations=770'),('0x0000000001010102','0x00000000003e0002'),('0x0000000040000050','0x0000000040000051'),('ok=1','ok=0'),('writes=1','writes=0')]:
            with self.assertRaises(ValueError):check(good.replace(old,new))
