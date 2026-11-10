"""Exercise the actual HDA/GT/reset/claim composition, including every fault prefix."""
import os
from pathlib import Path
import re
import subprocess
import unittest

EXE = Path(os.environ.get('INTEL_HDA_GPU_EXE', Path(__file__).resolve().parents[1] / 'build/tests/intel-hda-gpu'))


class JointTests(unittest.TestCase):
    def run_case(self, mode, point=1):
        p = subprocess.run([str(EXE), mode, str(point)], capture_output=True, text=True, timeout=10)
        self.assertEqual(p.returncode, 0, (mode, point, p.stdout, p.stderr))
        self.assertIn('hda-gpu: PASS', p.stdout)
        return {k: int(v) for k, v in re.findall(r'(clocks|guards|endpoints|reads|claim_reads|attempts|stores|checks)=(\d+)', p.stdout)}

    def test_original_lifetime_and_every_failure_prefix(self):
        normal = self.run_case('normal')
        self.assertEqual((normal['reads'], normal['claim_reads'], normal['attempts'], normal['stores']), (232, 12, 2, 2))
        self.assertGreater(normal['checks'], 0)
        cases = 1
        for mode in ('reentry', 'clean-refusal', 'false-pass', 'claim-live', 'lost-hda-list', 'lost-hda-child', 'lost-hda-count', 'mid-fail', 'retire'):
            with self.subTest(mode=mode):
                self.run_case(mode)
            cases += 1
        for mode in ('zero', 'backward', 'backstep', 'hda-deadline', 'outer-deadline'):
            # Samples 1/2 precede assignment of the original HDA origin.
            for point in range(3 if mode == 'hda-deadline' else 1, normal['clocks'] + 1):
                with self.subTest(mode=mode, point=point):
                    self.run_case(mode, point)
                cases += 1
        for mode, count in (('guard', normal['guards']), ('endpoint', normal['endpoints']), ('endpoint-drift', normal['endpoints']), ('hda-read', normal['reads']), ('hda-drift', normal['reads']), ('none', 2), ('uncertain', 2)):
            for point in range(1, count + 1):
                with self.subTest(mode=mode, point=point):
                    self.run_case(mode, point)
                cases += 1
        print(f'joint lifetime: {cases} actual-core cases, {normal["clocks"]} clock and {normal["guards"]} HDA guard positions')


if __name__ == '__main__':
    unittest.main()
