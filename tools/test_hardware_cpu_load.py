import unittest
import hardware_boot as boot
from hardware_cpu_load import load_result

class LoadCaptureTests(unittest.TestCase):
    def fixture(self):
        lines=['cpu-load-capture: returned=yes process_ok=yes']
        for i,name in enumerate(('idle','load','recovery')):
            start=i*30000000
            lines.append(f'cpu-load: phase={name} completed=yes elapsed_us=30000000 start_us={start} end_us={start+30000000} chunks={100 if i==1 else 0} checksum=42')
            for offset in (5000000,15000000,25000000):
                t=start+offset;n=t//1000000
                lines.append(f'workspace-devices: power_state=2 flags=3 energy_shift=14 nominal_mhz=2500 sample_us={t} interval_us=1000000 samples={n} rejected=0 energy={n*16384} aperf={n*1000} mperf={n*1000} monotonic_us={t+100}')
        return '\n'.join(lines)+'\ncpu-load: completed=yes\n'
    def test_complete_and_phase_power(self):
        result=load_result(self.fixture());self.assertTrue(result['completed'])
        for p in result['phases']:
            self.assertEqual(p['power']['average_package_mw'],1000)
            self.assertEqual(p['power']['covered_us'],20000000)
    def test_incomplete_failed_and_invalid(self):
        text=self.fixture()
        for broken in (text.replace('cpu-load: completed=yes\n',''),text+'cpu-load: completed=NO reason=temperature-stop\n',text.replace('chunks=100','chunks=0'),text.replace('start_us=30000000','start_us=30000001'),text.replace('elapsed_us=30000000','elapsed_us=29000000')):
            self.assertFalse(load_result(broken)['completed'])
    def test_bad_power_does_not_become_measurement(self):
        for broken in (self.fixture().replace('flags=3','flags=0'),self.fixture().replace('rejected=0','rejected=1'),self.fixture().replace('energy_shift=14','energy_shift=13')):
            r=load_result(broken);self.assertTrue(r['completed']);self.assertTrue(all(p['power'] is None for p in r['phases']))
    def test_profile_is_distinct(self):
        v='ram-usb-v35-laptop-probe-12345678'
        self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v))
        c=boot.limine_config('a'*16,v)
        for token in ('srvros.capture=cpu-load','srvros.power=read-only','srvros.smc=read-only'):
            self.assertIn(token,c)
        for token in ('srvros.aml','srvros.capture=desktop-idle','srvros.desktop=trial'):
            self.assertNotIn(token,c)

if __name__=='__main__':unittest.main()
