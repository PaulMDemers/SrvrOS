"""Bind the isolated held requester stage to its compiled exact recipe."""
import subprocess
import unittest
from hardware_boot import limine_config, stage

class HeldStagingTests(unittest.TestCase):
    def test_recipe_matches_compiled_parser(self):
        config = limine_config('0123456789abcdef', 'ram-usb-v1-intel-held-requester-laptop-probe-37d8c5d0-auto-return')
        line = next(line.split('cmdline:', 1)[1].strip() for line in config.splitlines() if 'cmdline:' in line)
        self.assertEqual(len(line.split()), 17)
        self.assertEqual(len(set(line.split())), 17)
        self.assertIn('srvros.intel-held-requester=read-only', line)
        self.assertIn('srvros.intel-engines-prepare=request-cancel', line)
        self.assertNotIn('srvros.intel-requester=read-only', line)
        for parser, accepted in (('held-requester', True), ('engine-prepare', False), ('requester', False)):
            result = subprocess.run(['build/tests/intel-gt-boot', parser, line], capture_output=True)
            self.assertEqual(result.returncode == 0, accepted)

    def test_exclusive_profile_before_staging(self):
        for extra in ({'intel_requester': True}, {'intel_engine_prepare': True},
                      {'repeat': 2}, {'hold_on_failure': True}, {'desktop_scale': 2},
                      {'battery_capture': True}, {'desktop_volume': '37d8c5d0'},
                      {'aml_capture': True}, {'cpu_load_capture': True}):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                stage(laptop_probe_volume='37d8c5d0', intel_held_requester=True, **extra)
        with self.assertRaises(ValueError):
            stage(intel_held_requester=True)

if __name__ == '__main__':
    unittest.main()
