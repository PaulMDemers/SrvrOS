#!/usr/bin/env python3
"""Exercise the offline AML adapter against our own synthetic firmware."""
from pathlib import Path
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

    def probe(self,*paths,snapshot=True,darwin=False,aml=None):
        args=[str(BINARY)]
        if darwin: args+=['--darwin']
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
