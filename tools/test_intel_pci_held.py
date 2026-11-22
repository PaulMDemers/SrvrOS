"""Actual prepare-action PCI sampler; each failure runs with an independent pin."""
from pathlib import Path
import resource
import subprocess
import unittest

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
CORE = Path('build/tests/intel-pci-held')
MODES = 'normal master-off pending pending-transition status-transition reentry after-action copied-preparation elapsed frozen shared-budget final-epoch final-inactive'.split()
POINTS = {
    **{mode: range(1, 19) for mode in ('read', 'float', 'profile', 'read-epoch', 'read-detach', 'read-time')},
    **{mode: range(1, 5) for mode in ('guard', 'guard-epoch', 'guard-detach', 'guard-time', 'origin-change', 'action-inactive')},
    'exit-float': (1, 9, 18),
    'exit-read': (1, 9, 18),
    'clock-zero': range(1, 5),
    'clock-backward': range(2, 5),
}

class HeldPciTests(unittest.TestCase):
    def test_actual_core_lifetimes_and_raw_prefixes(self):
        for mode in MODES:
            with self.subTest(mode=mode):
                result = subprocess.run([str(CORE), mode], check=True, capture_output=True, text=True, timeout=15)
                self.assertIn('PASS', result.stdout)
        for mode, points in POINTS.items():
            for point in points:
                with self.subTest(mode=mode, point=point):
                    result = subprocess.run([str(CORE), mode, str(point)], check=True, capture_output=True, text=True, timeout=15)
                    self.assertIn('PASS', result.stdout)

if __name__ == '__main__':
    unittest.main()
