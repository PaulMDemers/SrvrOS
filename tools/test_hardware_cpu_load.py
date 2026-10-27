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
    def test_native_idle_shortfall_and_retained_coverage(self):
        # Real failing phase bounds/checkpoint times; the post-idle logger
        # snapshot cannot be used to round 9.999841s up to the 10s requirement.
        from hardware_cpu_load import phase_power
        def row(at, logged, prefix='workspace-devices:'):
            n=at//1000000
            return f'{prefix} power_state=2 flags=3 energy_shift=14 nominal_mhz=2000 sample_us={at} interval_us=999984 samples={n} rejected=0 energy={n*16384} aperf={n*1000} mperf={n*1000} monotonic_us={logged}'
        old='\n'.join((row(11349926,12073449),row(21349767,22179818),row(31349608,32289646)))
        self.assertIsNone(phase_power(old,2073586,32079598))
        extra='\n'.join(row(at,at,'cpu-load-power:') for at in (7349990,13349894,19349798,25349702))
        fixed=old+'\ncpu-load-power: retained=4 overflow=0\n'+extra
        self.assertGreaterEqual(phase_power(fixed,2073586,32079598)['covered_us'],10000000)
        for broken in (fixed.replace('retained=4','retained=3'),fixed.replace('overflow=0','overflow=1'),fixed.replace('retained=4 overflow=0','retained=4 overflow=0\ncpu-load-power: retained=4 overflow=0'),old+'\n'+extra):
            self.assertIsNone(phase_power(broken,2073586,32079598))

    def test_deferred_records_all_phases_and_stale_refusal(self):
        text=self.fixture().replace('workspace-devices:', 'cpu-load-power:')+'cpu-load-power: retained=9 overflow=0\n'
        result=load_result(text)
        self.assertTrue(result['completed'])
        self.assertTrue(all(p['power'] and p['power']['covered_us']==20000000 for p in result['phases']))
        for broken in (text.replace('flags=3','flags=bad',1),text.replace('flags=3','flags=4'),text.replace('energy_shift=14','energy_shift=13'),text.replace('rejected=0','rejected=1')):
            self.assertTrue(all(p['power'] is None for p in load_result(broken)['phases']))

    def residency_fixture(self):
        text=self.fixture().replace('workspace-devices:', 'cpu-load-power:')+'cpu-load-power: retained=9 overflow=0\n'
        for n in (5,15,25,35,45,55,65,75,85):
            fields=f'cpu-load-idle: supported=1023 valid=1023 rejected=0 sample_us={n*1000000} interval_us=1000000 interval_tsc=2000000000 tsc_begin={n*2000000000} tsc_end={n*2000000000}'
            fields+=''.join(f' c{i}_raw={n*200000000 if i==0 else 0} c{i}_bp={1000 if i==0 else 0}' for i in range(10))
            text+=fields+'\n'
        return text

    def test_residency_coverage_and_zero(self):
        from hardware_cpu_load import phase_idle
        result=load_result(self.residency_fixture())
        for p in result['phases']:
            self.assertEqual(p['idle_residency']['covered_us'],20000000)
            self.assertEqual(p['idle_residency']['basis_points']['core_c3'],1000)
            self.assertEqual(p['idle_residency']['basis_points']['package_c10'],0)
        self.assertIsNone(phase_idle(self.residency_fixture(),0,14999999))
        self.assertIsNone(phase_idle(self.fixture(),0,30000000))

    def test_residency_invalid_is_not_zero(self):
        from hardware_cpu_load import phase_idle
        text=self.residency_fixture()
        for old,new in [('valid=1023','valid=0'),('supported=1023','supported=7'),
                        ('valid=1023 rejected=0','valid=1023 rejected=1'),('retained=9','retained=8'),('overflow=0','overflow=1'),
                        ('c0_bp=1000','c0_bp=10001'),('c9_raw=0',f'c9_raw={2**60}'),
                        ('c0_raw=1000000000','c0_raw=999999999999999'),
                        ('c0_raw=3000000000','c0_raw=0'),
                        ('tsc_begin=30000000000','tsc_begin=1'),
                        ('sample_us=15000000 interval_us=1000000 interval_tsc','sample_us=15000001 interval_us=1000000 interval_tsc'),
                        ('c0_bp=1000','c0_bp=1000 c0_bp=1000'),('c0_bp=1000','c0_bp=bad')]:
            with self.subTest(old=old,new=new): self.assertIsNone(phase_idle(text.replace(old,new,1),0,30000000))
        self.assertIsNone(phase_idle(text+'cpu-load-idle: truncated\n',0,30000000))

    def test_profile_is_distinct(self):
        v='ram-usb-v35-laptop-probe-12345678'
        self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v))
        c=boot.limine_config('a'*16,v)
        for token in ('srvros.capture=cpu-load','srvros.power=read-only','srvros.smc=read-only'):
            self.assertIn(token,c)
        for token in ('srvros.aml','srvros.capture=desktop-idle','srvros.desktop=trial'):
            self.assertNotIn(token,c)

if __name__=='__main__':unittest.main()
