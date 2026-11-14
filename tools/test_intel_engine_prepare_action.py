"""Actual-core maintained read-only preparation action boundaries."""
from pathlib import Path
import subprocess
import unittest

MODES = ('normal', 'reentry', 'no-action', 'missing', 'refuse', 'epoch', 'seal',
         'parent', 'borrow-seal', 'detach', 'deadline', 'hold-drift', 'post-guard', 'post-time')

class PrepareActionTests(unittest.TestCase):
    def test_actual_core_lifetimes(self):
        binary = Path('build/tests/intel-engine-prepare-action')
        for mode in MODES:
            with self.subTest(mode=mode):
                result = subprocess.run([str(binary), mode], capture_output=True,
                                        text=True, timeout=5)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn('engine-prepare-action: PASS', result.stdout)

if __name__ == '__main__':
    unittest.main()
