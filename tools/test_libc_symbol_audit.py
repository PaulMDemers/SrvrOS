"""Check that the symbol audit cannot pass without inspectable declarations."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).with_name('libc_symbol_audit.py')

class Audit(unittest.TestCase):
    def run_audit(self, header, symbols):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'headers').mkdir()
            (root / 'headers' / 'fixture.h').write_bytes(header)
            (root / 'archive').touch()
            nm = root / 'nm'
            nm.write_text('#!/bin/sh\nprintf %b ' + repr(symbols) + '\n')
            nm.chmod(0o755)
            return subprocess.run([sys.executable, str(SCRIPT), '--include', str(root / 'headers'),
                '--archive', str(root / 'archive'), '--nm', str(nm), '--fail-missing'],
                capture_output=True, text=True)

    def test_empty_or_undecodable_header_fails(self):
        for data in (b'/* no declarations */\n', b'int \xffmissing(void);\n'):
            with self.subTest(data=data):
                self.assertNotEqual(self.run_audit(data, '').returncode, 0)

    def test_missing_and_present_export(self):
        missing = self.run_audit(b'int fixture(void);\n', '')
        self.assertNotEqual(missing.returncode, 0)
        self.assertIn('missing\tfixture', missing.stdout)
        present = self.run_audit(b'int fixture(void);\n', '00000000 T fixture\n')
        self.assertEqual(present.returncode, 0, present.stderr)
        self.assertIn('missing declared exports: 0', present.stdout)

if __name__ == '__main__':
    unittest.main()
