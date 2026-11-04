"""Evidence parsers must reject interpreters that remove their checks."""
import os
from pathlib import Path
import subprocess
import sys
import unittest

MODULES = (
    'battery_control_files_records', 'battery_fault_records',
    'battery_home_desktop_records', 'battery_home_records',
    'battery_native_query_records', 'battery_native_smbus_records',
    'battery_query_records', 'battery_smbus_transition_records',
    'battery_smbus_workspace_records', 'battery_workspace_controls_records',
    'desktop_power_records', 'file_service_records',
    'serial_records', 'desktop_foundation_smoke',
    'app_timing', 'clock_smoke', 'exfat_hardening_test', 'exfat_import_smoke',
    'exfat_scale_test', 'exfat_scratch_test', 'lifetime_timing', 'loop_timing',
)

class OptimizedRecords(unittest.TestCase):
    def test_optimized_import_fails_closed(self):
        env = dict(os.environ, PYTHONPATH=str(Path(__file__).resolve().parent))
        for module in MODULES:
            with self.subTest(module=module):
                result = subprocess.run([sys.executable, '-O', '-c', f'import {module}'],
                                        env=env, capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('evidence validation requires Python assertions', result.stderr)

if __name__ == '__main__':
    unittest.main()
