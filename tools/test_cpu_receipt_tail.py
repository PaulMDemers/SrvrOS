import unittest
from check_cpu_capture_tail import assess, final_layout, covered
import test_hardware_platform as platform_tests


class FinalReceiptTests(unittest.TestCase):
    def fixture(self, resources=False):
        t=platform_tests.PlatformTests()
        s=t.resource_fixture() if resources else t.display_fixture()
        lines=s.splitlines(keepends=True)
        header=next(x for x in lines if x.startswith('ap-owner: v=1 admitted=1'))
        lines.remove(header)
        # Match native emission: power/idle telemetry is already deferred by
        # power_capture_stop, and process return is reported after AP cleanup.
        telemetry=[x for x in lines if x.startswith(('cpu-load-power:','cpu-load-idle:','cpu-idle-','cpu-mwait'))]
        returned=[x for x in lines if x.startswith('cpu-load-capture:')]
        lines=[x for x in lines if x not in telemetry+returned]
        s=header+'cpu-load: version=1 phases_seconds=30,30,30 workers=1 control_writes=none\n'+''.join(lines)+''.join(returned)
        s=s.replace('cpu-load: completed=yes\n','cpu-load: completed=yes\n'+''.join(telemetry))+'laptop-capture: completed=yes\n'
        return s.replace('cpu-load: completed=yes\n','io-owner-fixture:'+'x'*100000+'\ncpu-load: completed=yes\n')

    def test_old_layout_fails_and_final_layout_keeps_all_proofs(self):
        s=self.fixture()
        self.assertFalse(covered(s[-65536:]))
        r=assess(s,True)
        self.assertEqual(r['status'],'COVERAGE_PASS')
        self.assertFalse(r['physical_acceptance'])
        self.assertFalse(r['original_exit_tail_coverage'])
        new=final_layout(s)
        for line in s.splitlines():
            if line.startswith(('cpu-load: phase=','ap-owner: v=1 admitted=1','cpu-load: version=')):
                self.assertIn(line,new)

    def test_resource_layout_retains_full_allowlist(self):
        r=assess(self.fixture(resources=True),True)
        self.assertEqual(r['status'],'COVERAGE_PASS')
        self.assertTrue(r['critical_records_preserved_byte_for_byte'])
        s=self.fixture(resources=True).replace('count=26 before=','count=25 before=')
        with self.assertRaises(ValueError):assess(s,True)

    def test_failed_or_missing_data_never_becomes_coverage(self):
        s=self.fixture()
        for bad in (s+'cpu-load: completed=NO reason=thermal-limit\n',
                    s.replace('ap-owner-cpu: id=2 seen=2','ap-owner-cpu: id=2 seen=3'),
                    s.replace('ctl=0,0,0,0','ctl=0,0,0,4294967295')):
            with self.assertRaises(ValueError):assess(bad,True)
        with self.assertRaises(ValueError):final_layout(s+s.splitlines()[0]+'\n')


if __name__=='__main__':unittest.main()
