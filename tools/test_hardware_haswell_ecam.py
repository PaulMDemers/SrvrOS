import base64
import hashlib
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import hardware_boot as boot
import hardware_haswell_ecam as ecam

SESSION = 'session-0000000000000001-00'
EXE = Path('build/tests/ecam-profile')
DMAR = bytearray(136)
DMAR[:4] = b'DMAR'; DMAR[4] = 136; DMAR[8] = 1; DMAR[9] = -sum(DMAR) % 256
PIN = hashlib.sha256(DMAR).hexdigest()


def record(prefix, fmt, values):
    return prefix+' data='+base64.b64encode(struct.pack('<'+fmt, *values)).decode()+'\n'


def fixture(maximum=False):
    early = subprocess.check_output([str(EXE), *(['max'] if maximum else [])], text=True)
    raw, report, _ = ecam.blob(early)
    with patch('hardware_haswell_firmware.PIN', PIN): p = ecam.decode(raw)
    ref = next(x for x in early.splitlines() if x.startswith('hsw-ecam-ref:'))+'\n'
    early = early.replace(ref, '')
    t = p['end']+1000
    late = ''
    f = p['firmware'][0]['samples'][0]
    for name, offset in [('hsw-firmware', 0), ('hcache-firmware-post', 5000)]:
        late += f'{name}: v=1 status=1 count={len(f["catalog"])} cap=128 dmar_bytes=136 limit_us=100000 mmio=0 writes=0 ownership=0\n'
        for phase in (0, 1):
            late += record(f'{name}-meta: v=1 phase={phase}', '6I3Q', f['identity']+[len(f['catalog']), f['table_pa'], t+offset+phase*200, t+offset+phase*200+100])
            late += f'{name}-dmar: v=1 phase={phase} data='+f['dmar_base64']+'\n'
            late += record(f'{name}-map: v=1 phase={phase}', f'{len(f["catalog"])*3}Q', [x for row in f['catalog'] for x in row])
    late += 'hsw-cache: v=1 status=1 samples=2 msr_cap=49 pte_cap=16 limit_us=100000 mappings=0 writes=0 ownership=0\n'
    for phase in (0, 1):
        c = p['cpus'][phase]['raw'].copy(); at=t+1000+phase*2500
        c[10:12] = [at+100, at+200]; c[15:17] = [at, at+300]; c[20] = 8
        late += record(f'hsw-cache-cpu: v=1 phase={phase}', '6I11Q4I43Q', c)
        for edge in (0, 1):
            for alias in (0, 1):
                va = 0xffffc001f3250000 if alias else p['hhdm']+0xfed90000
                entry = p['root_before'][384 if alias else 256]
                late += record(f'hsw-cache-walk: v=1 phase={phase} edge={edge} alias={alias}', '10Q3I', [va, 0, p['root'], entry, p['pdpt'] if alias else 0x2002000, 0, 0, 0, 0, 0, 2, 0, 1])
    a = p['pdpt_before'].copy(); a[12] = p['frame'] | 3
    pages = [(p['root'],0,p['root_before']), (p['pdpt'],1,a), (0x2002000,1,[0x2003003]+[0]*511),
             (0x2003000,2,[0]*16+[0x2004003]+[0]*495), (p['frame'],2,p['pd']),
             (0x2004000,3,[0x2000003+4096*i for i in range(8)]+[0]*504)]
    data = bytearray()
    for pa, level, words in pages:
        data += struct.pack('<QB', pa, level); i=0
        while i<512:
            n=1; step=0
            if i+1<512:
                step=(words[i+1]-words[i]) % (1<<64);n=2
                while i+n<512 and words[i+n] == (words[i]+n*step) % (1<<64):n+=1
            data += struct.pack('<HQQ', n, words[i], step);i+=n
    late += f'hsw-alias: v=1 status=1 step=6 pages=6 reads=3072,3072 bytes={len(data)},{len(data)} target_leaves=0 root=0x{p["root"]:016x} nx=0x0000000004100000,0x0000000004100000 max=0x0000000080000008,0x0000000080000008 efer=0x0000000000000d00,0x0000000000000d00 irq=0x0000000000000202,0x0000000000000202 begin={t+2000} end={t+3000} cursor=0x0000000002004000,511,0x0000000000000000,0x0000000000000000 table_cap=64 entry_cap=32768 data_cap=4096 limit_us=100000 scope=sole-managed-root writes=0 maps=0 ownership=0\n'
    for phase in (0,1):
        for i, at in enumerate(range(0,len(data),512)):
            late += f'hsw-alias-data: v=1 phase={phase} chunk={i} data='+base64.b64encode(data[at:at+512]).decode()+'\n'
    late += f'hsw-alias-epoch: v=1 phase=pre-user-init begin={t} end={t+6000} report={report+1000} attempted=1\n'
    return early, late+ref+'laptop-capture: completed=yes\n'


class EcamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not EXE.exists(): raise unittest.SkipTest('build real ECAM profile fixture first')

    def test_real_producer_and_joint_proof(self):
        for maximum in (False, True):
            a,b=fixture(maximum)
            with patch('hardware_haswell_firmware.PIN', PIN):
                p=ecam._result(a,b,SESSION)
            self.assertEqual(p['status'],'OBSERVED_BOOT_ECAM_AND_MANAGED_ROOT')
            self.assertEqual(p['alias_pages'],6)
            for key in ('hardware_authority','ownership','all_hardware_aliases_proven','native_mapping_authorized'):self.assertFalse(p[key])
            # Measured real formatter + audited unchanged capture postamble budget.
            self.assertLessEqual(len(a)+16514,65536)
            # A worst-case 4096-byte census adds at most 2*(8*720)
            # compared with this compact actual six-table graph.
            self.assertLessEqual(len(b)+16514+11520,65536)

    def test_collector_retains_and_requires_separate_file(self):
        from hardware_desktop import collect
        a,b=fixture()
        with tempfile.TemporaryDirectory() as tmp, patch('hardware_haswell_firmware.PIN',PIN):
            root=Path(tmp);mount=root/'usb';work=mount/'srvros-work';folder=work/'Logs'/SESSION
            folder.mkdir(parents=True)
            (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            (folder/'boot.log').write_text('cmdline: srvros.ecam=air61-2m-test srvros.desktop-release=0000000000000001\n')
            (folder/'ecam.log').write_text(a);(folder/'exit.log').write_text(b)
            _,report=collect(mount,'37d8c5d0',root/'out')
            self.assertTrue(report['sessions'][0]['ecam_boot']['source_completed'])
            (folder/'ecam.log').unlink()
            _,report=collect(mount,'37d8c5d0',root/'out')
            self.assertIsNone(report['sessions'][0]['ecam_boot'])
            self.assertTrue(any('dual-file' in row for row in report['sessions'][0]['attention']))

    def test_each_record_session_and_completion_required(self):
        a,b=fixture()
        with patch('hardware_haswell_firmware.PIN', PIN):
            for line in a.splitlines():
                self.assertIsNone(ecam.ecam_result(a.replace(line+'\n',''),b,SESSION))
                self.assertIsNone(ecam.ecam_result(a+line+'\n',b,SESSION))
            for old,new in [('completed=yes','completed=NO'),('saved=1','saved=0'),('adopted=1','adopted=0'),(SESSION,SESSION[:-1]+'1')]:
                self.assertIsNone(ecam.ecam_result(a,b.replace(old,new),SESSION))
            self.assertIsNone(ecam.ecam_result(a,b,SESSION[:-1]+'1'))

    def test_raw_corruption_rejected(self):
        a,b=fixture();data,_,_=ecam.blob(a)
        with patch('hardware_haswell_firmware.PIN', PIN):
            for i in (0,8,12,20,28,36,52,60,72,76,80,84,88,92,100,108,18938,len(data)-1):
                damaged=bytearray(data);damaged[i]^=1
                with self.assertRaises(ValueError,msg=str(i)):ecam.decode(damaged)

    def test_actual_large_hhdm_lifetime_accounting(self):
        for size in ('2m','1g'):
            a=subprocess.check_output([str(EXE),size],text=True)
            data,_,_=ecam.blob(a)
            with patch('hardware_haswell_firmware.PIN',PIN):p=ecam.decode(data)
            self.assertLess(p['words'],65536)

    def test_actual_terminal_report_preserves_first_failure_without_reread(self):
        a=subprocess.check_output([str(EXE),'failure'],text=True)
        self.assertIn('ECAM BOOT HALTED: fixture adoption failed\nstate=5 ',a)
        self.assertIn('ecam-failure: v=1 stage=adopt-tables reason=pdpt-word\n',a)
        self.assertIn('flags=3 table=0x0000000002001000 index=12 component=0\n',a)
        self.assertIn('expected=0x0000000002005003 observed=0x0000000002005023',a)
        self.assertIn('rejected_clock=0 elapsed_valid=1',a)
        self.assertIn('invalidations=39936 metadata=39936 firmware_bytes=1840 phases=2',a)
        self.assertIn('No PCI/USB continuation or table reuse.',a)
        self.assertNotIn('hsw-ecam: v=5',a)
        for mode, reason, valid in [('failure-clock','clock-invalid',0),('failure-deadline','lifetime-deadline',1)]:
            a=subprocess.check_output([str(EXE),mode],text=True)
            self.assertIn('stage=adopt-gate reason='+reason,a)
            self.assertIn(f'rejected_clock=1 elapsed_valid={valid}',a)
            if valid:self.assertIn('elapsed=100000 ',a)

    def test_v5_profile_requires_explicit_dependencies(self):
        conf=boot.limine_config('x','ram-usb-v5-hfw-laptop-probe-1234abcd-auto-return')
        self.assertIn('srvros.ecam=air61-2m-test',conf)
        self.assertIn('srvros.hsw-cache=alias-nx-preinit',conf)
        for version in range(1,5):
            self.assertNotIn('srvros.ecam=',boot.limine_config('x',f'ram-usb-v{version}-hfw-laptop-probe-1234abcd-auto-return'))
        self.assertRaises(ValueError,boot.stage,haswell_ecam_boot=True)
        self.assertRaises(ValueError,boot.stage,haswell_ecam_boot=True,haswell_alias_preinit=True)


if __name__=='__main__':unittest.main()
