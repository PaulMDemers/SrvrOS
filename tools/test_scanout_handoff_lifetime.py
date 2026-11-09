"""Actual handoff/exclusion core: one process per terminal ownership case."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/tests/scanout-handoff-lifetime"
MODES = """normal copy-close copy-reopen reenter-open reenter-close
release-during-open snapshot-input token-input provider-context provider-function
close-admission failed-open failed-close pool-exhausted poison-open poison-close
other-pin""".split()


class HandoffLifetimeTests(unittest.TestCase):
    def test_ownership_cases(self):
        for mode in MODES:
            with self.subTest(mode=mode):
                run = subprocess.run([str(EXE), mode], capture_output=True,
                                     text=True, timeout=5)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                self.assertIn("PASS", run.stdout)


if __name__ == "__main__":
    unittest.main()
