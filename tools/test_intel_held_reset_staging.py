"""Bind the isolated held reset stage to its compiled exact recipe."""
import subprocess
import unittest
from hardware_boot import limine_config, stage

class HeldResetStagingTests(unittest.TestCase):
    def test_recipe_matches_compiled_parser(self):
        config = limine_config('0123456789abcdef', 'ram-usb-v1-intel-held-reset-laptop-probe-37d8c5d0-auto-return')
        line = next(line.split('cmdline:', 1)[1].strip() for line in config.splitlines() if 'cmdline:' in line)
        self.assertEqual(len(line.split()), 18)
        self.assertEqual(len(set(line.split())), 18)
        self.assertIn('srvros.intel-held-requester=read-only', line)
        self.assertIn('srvros.intel-engines-prepare=request-cancel', line)
        self.assertNotIn('srvros.intel-requester=read-only', line)
        for parser, accepted in (('held-reset', True), ('held-requester', False), ('engine-prepare', False), ('requester', False)):
            result = subprocess.run(['build/tests/intel-gt-boot', parser, line], capture_output=True)
            self.assertEqual(result.returncode == 0, accepted)

    def test_missing_duplicate_foreign_and_untrusted_recipe(self):
        config=limine_config('0123456789abcdef','ram-usb-v1-intel-held-reset-laptop-probe-37d8c5d0-auto-return')
        line=next(l.split('cmdline:',1)[1].strip() for l in config.splitlines() if 'cmdline:' in l)
        words=line.split()
        invalid=[' '.join(words[:n]+words[n+1:]) for n in range(len(words))]
        invalid += [line+' '+word for word in words]
        invalid += [line+' '+word for word in ('srvros.aml=probe','srvros.ap=probe','srvros.intel-bcs-reset=blitter-only','srvros.intel-requester=read-only')]
        invalid += [line.replace('37d8c5d0','37D8C5D0'),line.replace('0123456789abcdef','0123456789abcdeg')]
        for value in invalid:
            with self.subTest(value=value):
                self.assertNotEqual(subprocess.run(['build/tests/intel-gt-boot','held-reset',value],capture_output=True).returncode,0)

    def test_exclusive_profile_before_staging(self):
        for extra in ({'intel_held_requester': True}, {'intel_requester': True}, {'intel_engine_prepare': True},
                      {'repeat': 2}, {'hold_on_failure': True}, {'desktop_scale': 2},
                      {'battery_capture': True}, {'desktop_volume': '37d8c5d0'},
                      {'aml_capture': True}, {'cpu_load_capture': True}):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                stage(laptop_probe_volume='37d8c5d0', intel_held_reset=True, **extra)
        with self.assertRaises(ValueError):
            stage(intel_held_reset=True)

if __name__ == '__main__':
    unittest.main()
