"""Actual-core same-borrow reset composition, prefixes and retained failures."""
from pathlib import Path
import re
import subprocess
import unittest

BINARY = Path('build/tests/intel-engine-held-reset')

class HeldResetCoreTests(unittest.TestCase):
    def run_case(self, mode, point=None):
        command = [str(BINARY), mode] + ([] if point is None else [str(point)])
        result = subprocess.run(command, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('engine-held-reset: PASS', result.stdout)
        return result.stdout

    def test_real_core_lifetimes(self):
        normal = self.run_case('normal')
        counts = re.search(r'clocks=(\d+) guards=(\d+)', normal)
        self.assertIsNotNone(counts)
        cases = [(m, None) for m in ('wait', 'reentry', 'missing', 'overflow', 'action-refuse',
                 'begin', 'end', 'end-time', 'near-deadline', 'never-ack',
                 'settle-frozen', 'mixed-bcs', 'preserve')]
        for mode in ('zero', 'backward', 'clock-epoch'):
            cases += [(mode, n) for n in range(1, int(counts[1])+1)]
        for mode in ('guard', 'guard-time', 'guard-epoch', 'guard-seal',
                     'guard-borrow', 'guard-detach', 'guard-census'):
            cases += [(mode, n) for n in range(1, int(counts[2])+1)]
        for mode in ('snapshot-read', 'snapshot-value', 'snapshot-float', 'snapshot-value-guard'):
            cases += [(mode, n) for n in range(1, 19)]
        for mode in ('snapshot-count', 'snapshot-epoch', 'write-none',
                     'write-uncertain', 'write-epoch', 'write-time',
                     'phase-time', 'ack-foreign', 'ack-float', 'poll-read',
                     'poll-foreign', 'ack-foreign-guard', 'poll-foreign-guard'):
            cases += [(mode, n) for n in range(1, 3)]
        cases += [('mixed-drift', n) for n in range(4)]
        for mode, point in cases:
            with self.subTest(mode=mode, point=point):
                self.run_case(mode, point)

if __name__ == '__main__':
    unittest.main()
