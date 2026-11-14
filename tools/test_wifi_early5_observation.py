import os
from pathlib import Path
import subprocess
import unittest

from wifi_early5_observation import observe


class EarlyObservationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = Path(os.environ.get('WIFI_EARLY5_BINARY', 'build/tests/xhci-cal5-mcu-host'))
        if not cls.binary.is_file():
            raise RuntimeError('Build the actual native early5 fixture before testing its decoder')
        cls.logs = [subprocess.check_output([str(cls.binary.resolve()), '--trace-early5', str(i)],
                                           text=True) for i in range(4)]

    def test_actual_native_emission(self):
        for i, log in enumerate(self.logs):
            with self.subTest(i=i):
                r = observe(log)
                self.assertEqual(r['summary']['complete'], i in (0, 2))
                self.assertEqual(r['calibration_queue_exhausted'], i == 2)
                self.assertFalse(r['native_accepted'] or r['root_cause_proven'] or
                                 r['dma_retirement_proven'] or r['recovery_authorized'])
        self.assertEqual(self.logs[1].count('wifi-mcu-loss-summary:'), 1)

    def test_legacy_absence(self):
        self.assertFalse(observe('old capture')['present'])
        self.assertFalse(observe('wifi-early5-boot: calibration_only=1 net_init_deferred=1 worker_deferred=1\n')['present'])

    def test_schema_and_forbidden_authority(self):
        text = self.logs[0]
        for old, new in (('version=1 complete=', 'version=2 complete='),
                         ('retained=1 reused=0', 'retained=1 reused=1'),
                         ('inspected=0 network_out=0', 'inspected=1 network_out=0'),
                         ('inspected=0 network_out=0', 'inspected=0 network_out=1'),
                         ('before_mac=1', 'before_mac=0'),
                         ('exhausted=0', 'exhausted=1'),
                         ('calibration=1 commands=5', 'calibration=1 commands=4'),
                         ('stopped=1 frozen=1', 'stopped=0 frozen=1')):
            self.assertIn(old, text)
            with self.subTest(new=new), self.assertRaises(ValueError):
                observe(text.replace(old, new))

    def test_partial_duplicate_misordered(self):
        text = self.logs[0]
        rows = [l for l in text.splitlines() if l.startswith('wifi-early5')]
        for line in rows:
            with self.subTest(missing=line), self.assertRaises(ValueError):
                observe(text.replace(line + '\n', ''))
            with self.subTest(duplicate=line), self.assertRaises(ValueError):
                observe(text + line + '\n')
        with self.assertRaises(ValueError):
            observe(text + 'wifi-early5-unknown: version=1\n')

    def test_failure_not_promoted(self):
        for log in (self.logs[1], self.logs[3]):
            with self.assertRaises(ValueError):
                observe(log.replace('wifi-early5: version=1 complete=0',
                                    'wifi-early5: version=1 complete=1'))


if __name__ == '__main__':
    unittest.main()
