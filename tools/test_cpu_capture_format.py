import unittest
from cpu_capture_format import compact_legacy, expand_capture, POWER_KEYS
from hardware_cpu_load import load_result, phase_power
from hardware_platform import platform_series_result
from check_cpu_capture_tail import covered, assess
import test_cpu_receipt_tail as tail_tests


class CompactCaptureTests(unittest.TestCase):
    def fixture(self):
        s=tail_tests.final_layout(tail_tests.FinalReceiptTests().fixture(resources=True))
        lines=[]
        for line in s.splitlines():
            if line.startswith('cpu-load-power: power_state='):
                fields=dict(x.split('=') for x in line.split()[1:])
                fields.update(package_mw='1000',active_mhz='2500',package_uj='1000000')
                line='cpu-load-power: '+' '.join(k+'='+fields[k] for k in POWER_KEYS)
            lines.append(line)
        return '\n'.join(lines)+'\n' 

    def test_exact_roundtrip_and_identical_semantics(self):
        old=self.fixture();new=compact_legacy(old)
        self.assertEqual(expand_capture(new),old)
        self.assertEqual(load_result(new),load_result(old))
        self.assertEqual(platform_series_result(new),platform_series_result(old))
        self.assertTrue(covered(new));self.assertLess(len(new),len(old))
        self.assertTrue(assess(new)['critical_records_preserved_byte_for_byte'])

    def test_power_only_schema_preserves_unavailable_audits(self):
        old='\n'.join(x for x in self.fixture().splitlines() if not x.startswith(('cpu-load-idle:','cpu-idle-entry:')))+'\n'
        new=compact_legacy(old)
        self.assertIn('cpu-capture-format: v=2 idle=0 entry=0',new)
        self.assertEqual(expand_capture(new),old)
        self.assertEqual(load_result(new),load_result(old))
        self.assertFalse(covered(new))

    def test_malformed_missing_duplicate_and_mixed_refuse(self):
        s=compact_legacy(self.fixture())
        p=next(x for x in s.splitlines() if x.startswith('cpu-p2:'))
        header=next(x for x in s.splitlines() if x.startswith('cpu-capture-format:'))
        legacy=next(x for x in self.fixture().splitlines() if x.startswith('cpu-load-power: power_state='))
        for bad in (s.replace(header+'\n',''),s+header+'\n',s+p+'\n',s+legacy+'\n',
                    s.replace('v=2 idle=1','v=3 idle=1'),s.replace('idle=1','idle=0'),
                    s.replace('i=1 v=','i=0 v='),s.replace(p+'\n',''),
                    s.replace('cpu-i2:','cpu-q2:'),s.replace('overflow=0','overflow=1'),
                    s.replace(p,p+','),s.replace(p,p.replace('v=','v='+str(2**64)+',',1))):
            with self.subTest(bad=bad[-70:]):
                with self.assertRaises(ValueError):expand_capture(bad)
                self.assertFalse(load_result(bad)['completed'])
                self.assertIsNone(phase_power(bad,0,30000000))
                self.assertFalse(covered(bad))

    def test_invalid_measurements_and_failure_markers_are_preserved(self):
        old=self.fixture()
        for bad in (old.replace('energy_shift=14','energy_shift=13'),
                    old.replace('valid=127','valid=0').replace('valid=1023','valid=0'),
                    old+'cpu-load: completed=NO reason=temperature-stop\n'):
            new=compact_legacy(bad)
            self.assertEqual(expand_capture(new),bad)
            self.assertEqual(load_result(new),load_result(bad))
            self.assertFalse(covered(new))


if __name__=='__main__':unittest.main()
