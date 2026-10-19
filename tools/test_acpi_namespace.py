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

    def probe(self,*paths,snapshot=True,darwin=False,aml=None,contract=None):
        args=[str(BINARY)]
        if darwin: args+=['--darwin']
        if contract: args+=['--ec-contract',*[str(p) for p in contract]]
        if snapshot: args+=['--region',f'0x100000:{self.snapshot}']
        args += [str(aml or self.aml),'--',*paths]
        return subprocess.run(args,capture_output=True,text=True,timeout=20)

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

    def test_snapshot_writes_are_private(self):
        before=self.snapshot.read_bytes()
        r=self.probe('\\READ','\\WRIT','\\READ')
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('\\READ = 0x21',r.stdout)
        self.assertIn('\\WRIT = 0x37',r.stdout)
        self.assertIn('\\READ = 0x37',r.stdout)
        self.assertEqual(self.snapshot.read_bytes(),before)

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
