import unittest,tempfile,shutil,os
from pathlib import Path
from mt76_trim5_reference import audit


class ReferenceTests(unittest.TestCase):
    def test_projection_and_damaged_sources(self):
        cache=os.environ.get('SRVROS_MT76_REFERENCE')
        if not cache:self.skipTest('set SRVROS_MT76_REFERENCE to the optional pinned source cache')
        reference=Path(cache)
        self.assertEqual(audit(reference)['selected'],0x44)
        with tempfile.TemporaryDirectory() as td:
            ref=Path(td)/'reference';shutil.copytree(reference,ref)
            for name in ('phy.c','eeprom0.c','eeprom.h'):
                p=ref/name;old=p.read_bytes();p.write_bytes(old+b'\n/* altered */\n')
                with self.assertRaisesRegex(ValueError,'source hash mismatch:'+name):audit(ref)
                p.write_bytes(old)
    def test_changed_fixture_or_dependency(self):
        cache=os.environ.get('SRVROS_MT76_REFERENCE')
        if not cache:self.skipTest('set SRVROS_MT76_REFERENCE to the optional pinned source cache')
        reference=Path(cache)
        with tempfile.TemporaryDirectory() as td:
            f=Path(td)
            for name in ('mt7610-5ghz-trim-cycle.json','mt7610-5ghz-trim-cycle.bin','mt7610-5ghz-band.bin','mt7610-5ghz-a20-hb.bin','mt7610-us5-channel161-frequency.bin','mt7610-5ghz-pa-cycle.bin','mt7610-5ghz-bbp-cycle.bin'):
                shutil.copy2(Path('tools/fixtures')/name,f/name)
            for name in ('mt7610-5ghz-trim-cycle.bin','mt7610-5ghz-band.bin'):
                p=f/name;old=p.read_bytes();b=bytearray(old);b[0]^=1;p.write_bytes(b)
                with self.assertRaisesRegex(ValueError,'hash mismatch'):audit(reference,f)
                p.write_bytes(old)
