"""Fault coverage for the unbound BCS protocol; this never runs native MMIO."""
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build/tests/intel-bcs-reset'
MODES = '''normal mask delayed nonzero-epoch fresh-reset policy missing-io stale-epoch overflow active
pending flip tlb stale-parent copy other-borrow epoch-during-census
epoch-final-census epoch-admission epoch-ready-admission zero frozen
backward clock-jump phase-deadline foreign-epoch foreign-request initial-control
control-reserved control-float initial-domain domain-foreign domain-float
late-domain ready-timeout ready-lost cancel-timeout cancel-foreign busy post-drift
settle-timeout return-deadline census-drift context-drift'''.split()
POINT_MODES = ('control-none', 'control-uncertain', 'domain-none',
               'domain-uncertain', 'reset-timeout')


def run(mode, point=None, exe=EXE):
    args = [str(exe), mode]
    if point is not None:
        args.append(str(point))
    return subprocess.check_output(args, text=True)


class BcsResetTests(unittest.TestCase):
    def test_fault_modes(self):
        for mode in MODES:
            with self.subTest(mode=mode):
                self.assertIn('PASS', run(mode))
        for mode in POINT_MODES:
            for point in (1, 2):
                with self.subTest(mode=mode, point=point):
                    self.assertIn('PASS', run(mode, point))

    def test_every_guard_and_read(self):
        baseline = run('normal')
        guards, reads = map(int, re.search(r'guards=(\d+) reads=(\d+)', baseline).groups())
        self.assertGreater(guards, 20)
        self.assertEqual(reads, 10)
        for mode, count in (('guard', guards), ('read', reads)):
            for point in range(1, count + 2):
                with self.subTest(mode=mode, point=point):
                    self.assertIn('PASS', run(mode, point))

    def test_no_native_entry(self):
        # Runtime admission and a real exact-reg adapter are separate work.
        import hardware_boot
        with self.assertRaises(TypeError):
            hardware_boot.stage(laptop_probe_volume='37d8c5d0', intel_bcs_reset_trial=True)
        makefile = (ROOT / 'Makefile').read_text()
        self.assertNotIn('build/intel-bcs-reset/srvros.elf:', makefile)
        self.assertNotIn('intel_bcs_reset.c', ' '.join(str(p) for p in (ROOT / 'kernel/src').rglob('*.c')))


if __name__ == '__main__':
    unittest.main()
