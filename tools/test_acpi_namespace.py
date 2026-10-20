#!/usr/bin/env python3
"""Exercise the offline AML adapter against our own synthetic firmware."""
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
BINARY=ROOT/'build/tools/acpi-namespace'

class NamespaceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory(prefix='srvros-aml-')
        cls.folder=Path(cls.tmp.name)
        cls.aml=cls.folder/'fixture.aml'
        result=subprocess.run(['iasl','-p',str(cls.folder/'fixture'),str(ROOT/'tools/fixtures/acpi/namespace.asl')],capture_output=True,text=True)
        if result.returncode: raise RuntimeError(result.stdout+result.stderr)
        cls.snapshot=cls.folder/'snapshot.bin'
        cls.snapshot.write_bytes(b'\x21'+bytes(15))

    @classmethod
    def tearDownClass(cls): cls.tmp.cleanup()

    def contract(self, *, uid=0, gpe=0x4e, path=b'\\_SB.EC\0'):
        def finish(data, signature):
            data[:4]=signature
            struct.pack_into('<I',data,4,len(data))
            data[9]=(-sum(data))&255
            return data
        ec=bytearray(65)+path
        for offset,address in [(36,0x66),(48,0x62)]:
            ec[offset:offset+4]=bytes([1,8,0,1])
            struct.pack_into('<Q',ec,offset+4,address)
        struct.pack_into('<I',ec,60,uid);ec[64]=gpe
        fa=bytearray(116);fa[46]=9;fa[89]=2;fa[92]=32
        struct.pack_into('<I',fa,64,0x1804);struct.pack_into('<I',fa,80,0x1880)
        e=self.folder/'ECDT.dat';f=self.folder/'FACP.dat'
        e.write_bytes(finish(ec,b'ECDT'));f.write_bytes(finish(fa,b'FACP'))
        return e,f

    def variant(self, before, after, rename=None):
        source=(ROOT/'tools/fixtures/acpi/namespace.asl').read_text()
        self.assertIn(before,source)
        asl=self.folder/'variant.asl';asl.write_text(source.replace(before,after))
        r=subprocess.run(['iasl','-p',str(self.folder/'variant'),str(asl)],capture_output=True,text=True)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        aml=self.folder/'variant.aml'
        if rename:
            # Construct malformed firmware after the ASL compiler's type checks.
            data=bytearray(aml.read_bytes());old,new=rename
            self.assertEqual(data.count(old),1)
            data=data.replace(old,new);data[9]=0;data[9]=(-sum(data))&255
            aml.write_bytes(data)
        return aml

    def probe(self,*paths,snapshot=True,darwin=False,aml=None,contract=None,models=(),set_models=(),pci=(),initialize=False):
        args=[str(BINARY)]
        if darwin: args+=['--darwin']
        if initialize: args+=['--initialize']
        for address,path in models: args+=['--io-model',f'{address}:{path}']
        for address,mask,path in set_models: args+=['--io-set-model',f'{address}:{mask}:{path}']
        for address,path in pci: args+=['--pci',f'{address}:{path}']
        if contract: args+=['--ec-contract',*[str(p) for p in contract]]
        if snapshot: args+=['--region',f'0x100000:{self.snapshot}']
        args += [str(aml or self.aml),'--',*paths]
        return subprocess.run(args,capture_output=True,text=True,timeout=20)

    def test_namespace_initialization_is_opt_in(self):
        aml=self.variant('Method (READ, 0)', 'Method (_INI, 0) { VAL0 = 0x42 } Method (READ, 0)')
        before=self.snapshot.read_bytes()
        r=self.probe('\\READ',aml=aml)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('\\READ = 0x21',r.stdout)
        self.assertNotIn('namespace-init:',r.stdout)
        r=self.probe('\\READ',aml=aml,initialize=True)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('namespace-init: PASS api=0 errors=0 warnings=0 denied=0',r.stdout)
        self.assertIn('\\READ = 0x42',r.stdout)
        self.assertIn('remaining_allocated=0',r.stdout)
        self.assertEqual(self.snapshot.read_bytes(),before)

    def test_namespace_initialization_ignored_error_refused(self):
        aml=self.variant('Method (READ, 0)', 'Method (_INI, 0) { IOV0 = One } Method (READ, 0)')
        r=self.probe('\\READ',aml=aml,initialize=True,contract=self.contract())
        self.assertEqual(r.returncode,1,r.stdout+r.stderr)
        self.assertIn('namespace-init: REFUSED api=0',r.stdout)
        self.assertIn('follow-up evaluations and EC contract skipped',r.stdout)
        self.assertNotIn('\\READ =',r.stdout)
        self.assertNotIn('ec-contract: MATCH',r.stdout)
        self.assertIn('remaining_allocated=0',r.stdout)

    def test_namespace_initialization_prior_load_fault_refused(self):
        aml=self.variant('Method (BADR, 0)', 'Name (BOOT, Zero) BOOT = IOV0 Method (BADR, 0)')
        r=self.probe('\\READ',aml=aml,initialize=True,contract=self.contract())
        self.assertEqual(r.returncode,1,r.stdout+r.stderr)
        self.assertIn('namespace-init: SKIPPED prior load errors=',r.stdout)
        self.assertNotIn('\\READ =',r.stdout)
        self.assertNotIn('ec-contract: MATCH',r.stdout)
        self.assertIn('remaining_allocated=0',r.stdout)

    def test_namespace_initialization_bad_status_refused(self):
        aml=self.variant('Name (_UID, Zero)', 'Name (_UID, Zero) Method (XSTA, 0) { Return (Buffer () {1}) }', (b'XSTA',b'_STA'))
        r=self.probe('\\READ',aml=aml,initialize=True)
        self.assertEqual(r.returncode,1,r.stdout+r.stderr)
        self.assertIn('namespace-init: REFUSED api=0',r.stdout)
        self.assertNotIn('\\READ =',r.stdout)
        self.assertIn('remaining_allocated=0',r.stdout)

    def test_resources_and_osi(self):
        r=self.probe('\\_SB.EC._HID','\\_SB.EC._CRS','\\_SB.EC.SMB0._STA','\\_SB.EC.SMB0._EC',darwin=True)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('0x90cd041',r.stdout)
        self.assertIn('47 01 62 00 62 00 00 01 47 01 66 00 66 00 00 01 79 00',r.stdout)
        self.assertIn('._STA = 0xf',r.stdout)
        self.assertIn('._EC = 0x2010',r.stdout)
        self.assertIn('denied=0',r.stdout)
        self.assertIn('remaining_allocated=0 errors=0',r.stdout)
        default=self.probe('\\_SB.EC.SMB0._STA')
        self.assertEqual(default.returncode,0,default.stderr)
        self.assertIn('._STA = 0x0',default.stdout)

    def test_ec_contract_match(self):
        r=self.probe('\\READ',contract=self.contract())
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('ec-contract: MATCH',r.stdout)
        self.assertIn('global_lock=absent hardware_access=none',r.stdout)
        self.assertIn('denied=0',r.stdout)

    def test_ec_contract_table_disagreements(self):
        for kwargs in [dict(uid=1),dict(gpe=0x4f),dict(path=b'\\_SB.MISS\0')]:
            with self.subTest(kwargs=kwargs):
                r=self.probe('\\READ',contract=self.contract(**kwargs))
                self.assertEqual(r.returncode,1,r.stdout+r.stderr)
                self.assertIn('ec-contract: REFUSED',r.stdout)
                self.assertIn('remaining_allocated=0',r.stdout)

    def test_ec_contract_namespace_disagreements(self):
        for before,after,rename in [
            ('Device (EC)','ThermalZone (EC)',None),
            ('EisaId ("PNP0C09")','EisaId ("PNP0C02")',None),
            ('Name (_UID, Zero)','Name (_UID, "0")',None),
            ('Name (_UID, Zero)','Name (_UID, 0x100000000)',None),
            ('Name (_UID, Zero)','Name (_UID, Zero) Name (_GLK, 2)',None),
            ('Name (_GPE, 0x4E)','Name (XGPE, Package () {0x4E})',(b'XGPE',b'_GPE')),
            ('IO (Decode16, 0x62, 0x62, 0, 1)','IO (Decode16, 0x63, 0x63, 0, 1)',None),
            ('Name (_UID, Zero)','Name (_UID, Zero) Name (XGLK, "bad")',(b'XGLK',b'_GLK')),
        ]:
            with self.subTest(after=after):
                r=self.probe('\\READ',contract=self.contract(),aml=self.variant(before,after,rename))
                self.assertEqual(r.returncode,1,r.stdout+r.stderr)
                self.assertIn('ec-contract: REFUSED',r.stdout)

    def test_ec_contract_string_hid_and_global_lock(self):
        aml=self.variant('Name (_HID, EisaId ("PNP0C09"))','Name (_HID, "PNP0C09") Name (_GLK, One)')
        r=self.probe('\\READ',contract=self.contract(),aml=aml)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('ec-contract: MATCH',r.stdout)
        self.assertIn('global_lock=required hardware_access=none',r.stdout)

    def test_ec_contract_bad_table(self):
        e,f=self.contract();bad=bytearray(e.read_bytes());bad[9]^=1;e.write_bytes(bad)
        r=self.probe('\\READ',contract=(e,f))
        self.assertEqual(r.returncode,1)
        self.assertIn('EC contract table refusal',r.stderr)
        self.assertNotIn('\\READ =',r.stdout)

    def test_io_model_preserve_and_private_file(self):
        model=self.folder/'gpio.bin'
        for value in [0,0x08,0xa5,0xff]:
            with self.subTest(value=value):
                model.write_bytes(bytes([value]))
                r=self.probe('\\GINI','\\GCLR',models=[('0xb90',model)])
                self.assertEqual(r.returncode,0,r.stdout+r.stderr)
                self.assertIn(f'write port=0xb90 width=8 before=0x{value:x} value=0x{value|8:x}',r.stdout)
                self.assertIn(f'write port=0xb90 width=8 before=0x{value|8:x} value=0x{value&~8:x}',r.stdout)
                self.assertIn('denied=0',r.stdout)
                self.assertIn('remaining_allocated=0',r.stdout)
                self.assertEqual(model.read_bytes(),bytes([value]))

    def test_io_model_accesses_remain_bounded(self):
        model=self.folder/'gpio.bin';model.write_bytes(b'\xa5')
        for method in ['\\GOUT','\\GWID']:
            with self.subTest(method=method):
                r=self.probe(method,models=[('0xb90',model)])
                self.assertEqual(r.returncode,1,r.stdout+r.stderr)
                self.assertIn('SystemIO access denied',r.stderr)
                self.assertIn('remaining_allocated=0',r.stdout)
                self.assertNotIn('io-model: write',r.stdout)
                self.assertEqual(model.read_bytes(),b'\xa5')

    def test_io_set_model_limits_changes(self):
        model=self.folder/'gpio.bin';model.write_bytes(bytes([0xa5]))
        r=self.probe('\\GINI',set_models=[('0xb90','8',model)])
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('write port=0xb90 width=8 before=0xa5 value=0xad',r.stdout)
        r=self.probe('\\GINI','\\GCLR',set_models=[('0xb90','8',model)])
        self.assertEqual(r.returncode,1,r.stdout+r.stderr)
        self.assertEqual(r.stdout.count('io-model: write'),1)
        self.assertIn('SystemIO access denied',r.stderr)
        self.assertIn('remaining_allocated=0',r.stdout)
        r=self.probe('\\GINI',set_models=[('0xb90','4',model)])
        self.assertEqual(r.returncode,1,r.stdout+r.stderr)
        self.assertNotIn('io-model: write',r.stdout)
        self.assertEqual(model.read_bytes(),bytes([0xa5]))
        for mask in ['0','256','-1','x']:
            r=self.probe('\\GINI',set_models=[('0xb90',mask,model)])
            self.assertEqual(r.returncode,2,r.stdout+r.stderr)
        model.write_bytes(bytes(2))
        r=self.probe('\\GINI',set_models=[('0xb90','8',model)])
        self.assertEqual(r.returncode,2,r.stdout+r.stderr)

    def test_io_model_word_endianness(self):
        model=self.folder/'word.bin';model.write_bytes(bytes(4))
        r=self.probe('\\GWID',models=[('0xb90',model)])
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('write port=0xb90 width=32 before=0x0 value=0x12345678',r.stdout)
        self.assertIn('\\GWID = 0x12345678',r.stdout)
        self.assertEqual(model.read_bytes(),bytes(4))

    def test_io_model_invalid_layout(self):
        model=self.folder/'gpio.bin';model.write_bytes(bytes(2))
        for models in [[('0xffff',model)],[('0xb90',model),('0xb91',model)],[('-1',model)]]:
            with self.subTest(models=models):
                r=self.probe('\\GINI',models=models)
                self.assertEqual(r.returncode,2,r.stdout+r.stderr)
                self.assertNotIn('io-model:',r.stdout)

    def test_io_model_access_budget(self):
        model=self.folder/'gpio.bin';model.write_bytes(b'\0')
        r=self.probe('\\GBUD',models=[('0xb90',model)])
        self.assertEqual(r.returncode,2,r.stdout+r.stderr)
        self.assertIn('SystemIO model access budget exceeded',r.stderr)
        self.assertEqual(r.stdout.count('io-model:'),4096)

    def test_darwin_policy_excludes_other_vendor_interfaces(self):
        r=self.probe('\\OPOL',darwin=True)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('\\OPOL = 0x1',r.stdout)

    def test_snapshot_writes_are_private(self):
        before=self.snapshot.read_bytes()
        r=self.probe('\\READ','\\WRIT','\\READ')
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('\\READ = 0x21',r.stdout)
        self.assertIn('\\WRIT = 0x37',r.stdout)
        self.assertIn('\\READ = 0x37',r.stdout)
        self.assertEqual(self.snapshot.read_bytes(),before)
        self.assertIn('memory-model: read address=0x100000 width=8 before=0x21 value=0x21',r.stdout)
        self.assertIn('memory-model: write address=0x100000 width=8 before=0x21 value=0x37',r.stdout)

    def test_wide_memory_trace(self):
        before=self.snapshot.read_bytes()
        r=self.probe('\\WR64')
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('memory-model: write address=0x100008 width=64 before=0x0 value=0xfedcba9876543210',r.stdout)
        self.assertIn('memory-model: read address=0x100008 width=64 before=0xfedcba9876543210 value=0xfedcba9876543210',r.stdout)
        self.assertEqual(self.snapshot.read_bytes(),before)

    def test_pci_trace_is_private(self):
        p=self.folder/'pci.bin';before=bytes(256);p.write_bytes(before)
        r=self.probe('\\_SB.PCI0.DEV0.PWR0',pci=[('0:0:1f.3',p)])
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('pci-model: write device=0:0:1f.3 offset=0x40 width=8 before=0x0 value=0xa5',r.stdout)
        self.assertIn('pci-model: read device=0:0:1f.3 offset=0x40 width=8 before=0xa5 value=0xa5',r.stdout)
        self.assertEqual(p.read_bytes(),before)

    def test_snapshot_trace_budget(self):
        p=self.folder/'pci.bin';p.write_bytes(bytes(256))
        for method,kind in [('\\MBUD','memory'),('\\_SB.PCI0.DEV0.PBUD','pci')]:
            with self.subTest(method=method):
                r=self.probe(method,pci=[('0:0:1f.3',p)])
                self.assertEqual(r.returncode,2,r.stdout+r.stderr)
                self.assertIn('memory/PCI snapshot access budget exceeded',r.stderr)
                self.assertEqual(r.stdout.count(kind+'-model:'),4096)

    def test_unmapped_memory_and_io_are_denied(self):
        for name,snapshot in [('\\READ',False),('\\BADR',True)]:
            with self.subTest(name=name):
                r=self.probe(name,snapshot=snapshot)
                self.assertNotEqual(r.returncode,0,r.stdout+r.stderr)
                self.assertIn('ERROR',r.stdout)
                self.assertIn('denied=1',r.stdout)

    def test_missing_object_is_failure(self):
        r=self.probe('\\MISS')
        self.assertNotEqual(r.returncode,0)
        self.assertIn('ERROR not found',r.stdout)

    def test_bad_checksum_is_rejected(self):
        damaged=bytearray(self.aml.read_bytes());damaged[9]^=1
        bad=self.folder/'bad.aml';bad.write_bytes(damaged)
        r=self.probe('\\READ',aml=bad)
        self.assertEqual(r.returncode,2)
        self.assertIn('bad table checksum',r.stderr)

    def test_loop_is_bounded(self):
        r=self.probe('\\LOOP')
        self.assertEqual(r.returncode,1,r.stdout+r.stderr)
        self.assertIn('loop',r.stdout.lower())
        self.assertIn('remaining_allocated=0',r.stdout)

if __name__=='__main__': unittest.main()
