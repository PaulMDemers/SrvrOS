"""Each actual-core process owns its terminal exclusion state independently."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODES = """normal missing invalid failed-open clock-open stuck-open deadline-open
post-open-clock source-open changed-first changed-last pool-exhausted poison-open
input-mutation reentry source-live clock-live poison-live regress-live failed-close
clock-close changed-close full-buffers partial-write pending-readers reuse
capacity last-aperture-page one-page partial-restore lost-mapped-owner other-pin""".split()

class TransferTests(unittest.TestCase):
    def test_actual_core(self):
        for mode in MODES:
            with self.subTest(mode=mode):
                run = subprocess.run([str(ROOT/'build/tests/scanout-transfer'), mode],
                                     capture_output=True, text=True, timeout=10)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                self.assertIn('PASS', run.stdout)

if __name__ == '__main__':
    unittest.main()
