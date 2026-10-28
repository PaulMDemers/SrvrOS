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

    def test_broadwell_partial_residency(self):
        from hardware_cpu_load import phase_idle
        text=self.residency_fixture().replace('supported=1023 valid=1023','supported=127 valid=127')
        result=phase_idle(text,0,30000000)
        self.assertEqual(result['supported'],127)
        self.assertEqual(len(result['basis_points']),7)
        self.assertNotIn('package_c10',result['basis_points'])
        for broken in (text.replace('c7_raw=0','c7_raw=1',1),
                       text.replace('c8_bp=0','c8_bp=1',1),
                       text.replace('supported=127 valid=127','supported=1023 valid=1023',1),
                       text.replace('valid=127','valid=1023',1)):
            self.assertIsNone(phase_idle(broken,0,30000000))

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

    def audit_fixture(self):
        text=self.residency_fixture()
        text+='cpu-idle-audit: version=1 entry=sti-hlt-cli policy_writes=0 managed_cpus=1 other_cpus_known=0 monitor=1 leaf1_b=262144 leaf5_a=64 leaf5_b=64 leaf5_c=3 leaf5_d=286531872 e2_start=1032 e2_end=1032 epoch=1 enters=9001 returns=9000 ready=100\n'
        for n in (5,15,25,35,45,55,65,75,85):
            text+=f'cpu-idle-entry: sample_us={n*1000000} epoch=1 enters={n*100+1} returns={n*100} ready={n}\n'
        return text

    def test_idle_entry_and_firmware(self):
        from hardware_cpu_load import phase_idle_entry
        for p in load_result(self.audit_fixture())['phases']:
            d=p['idle_entry'];self.assertEqual(d['covered_us'],20000000);self.assertEqual(d['enters'],2000)
            self.assertEqual(d['returns'],2000);self.assertEqual(d['firmware_start']['limit'],'C10')
            self.assertEqual(d['firmware_start']['io_mwait_redirection'],1)
            self.assertFalse(d['firmware_changed']);self.assertEqual(d['cpuid_logical_capacity'],4)
        changed=self.audit_fixture().replace('e2_end=1032','e2_end=32768')
        d=phase_idle_entry(changed,0,30000000);self.assertTrue(d['firmware_changed'])
        self.assertEqual(d['firmware_end']['limit'],'C0/C1');self.assertEqual(d['firmware_end']['cfg_lock'],1)
        d=phase_idle_entry(changed.replace('e2_end=32768','e2_end=15'),0,30000000)
        self.assertEqual(d['firmware_end']['limit'],'unknown')
        self.assertIsNone(phase_idle_entry(self.residency_fixture(),0,30000000))

    def test_idle_entry_refusals(self):
        from hardware_cpu_load import phase_idle_entry
        text=self.audit_fixture()
        for old,new in [('epoch=1 enters=501','epoch=2 enters=501'),('enters=501','enters=499'),
                        ('returns=500','returns=499'),('enters=1501','enters=500'),
                        ('sample_us=15000000 epoch','sample_us=15000001 epoch'),
                        ('policy_writes=0','policy_writes=1'),('entry=sti-hlt-cli','entry=mwait'),
                        ('monitor=1','monitor=0'),('other_cpus_known=0','other_cpus_known=1'),
                        ('enters=9001','enters=0'),('epoch=1 enters=9001','epoch=0 enters=9001'),
                        ('ready=100','ready=100 ready=100'),('ready=100','ready=bad')]:
            with self.subTest(old=old):self.assertIsNone(phase_idle_entry(text.replace(old,new,1),0,30000000))
        self.assertIsNone(phase_idle_entry(text+'cpu-idle-entry: truncated\n',0,30000000))
        self.assertIsNone(phase_idle_entry(text+'cpu-idle-audit: truncated\n',0,30000000))

    def prereq_fixture(self):
        return self.audit_fixture()+'cpu-idle-prereq: v=1 epoch=1 max=20 b0=1,2,256,0 b1=4,4,513,0 b2=0,0,2,0 apic=4276095232,4276095232 misc=8716425,8716425\n'

    def test_prerequisites(self):
        from hardware_cpu_load import idle_prerequisites
        p=idle_prerequisites(self.prereq_fixture())
        self.assertEqual(p['topology']['logical_per_core'],2)
        self.assertEqual(p['topology']['logical_per_package'],4)
        self.assertEqual(p['topology']['cores_per_package'],2)
        self.assertEqual(p['config_start']['bsp'],1)
        self.assertEqual(p['config_start']['x2apic_enabled'],0)
        self.assertTrue(p['monitor_fsm_consistent']);self.assertFalse(p['configuration_changed'])
        self.assertTrue(p['mwait_extensions'] and p['masked_interrupt_break'])
        self.assertEqual(p['other_cpus_state'],'unknown');self.assertFalse(p['remote_observation'])
        self.assertEqual(load_result(self.prereq_fixture())['idle_prerequisites'],p)
        changed=self.prereq_fixture().replace('misc=8716425,8716425','misc=8716425,0')
        self.assertTrue(idle_prerequisites(changed)['configuration_changed'])
        self.assertFalse(idle_prerequisites(changed)['monitor_fsm_consistent'])
        for old,new in [('b1=4,4,513,0','b1=4,4,769,0'),('b2=0,0,2,0','b2=5,8,1026,0'),('b1=4,4,513,0','b1=4,4,513,1')]:
            self.assertIsNone(idle_prerequisites(self.prereq_fixture().replace(old,new))['topology'])
        absent=self.prereq_fixture().replace('max=20','max=6').replace('b0=1,2,256,0','b0=0,0,0,0').replace('b1=4,4,513,0','b1=0,0,0,0').replace('b2=0,0,2,0','b2=0,0,0,0')
        self.assertIsNone(idle_prerequisites(absent)['topology'])

    def test_prerequisite_refusals(self):
        from hardware_cpu_load import idle_prerequisites
        text=self.prereq_fixture()
        for old,new in [('v=1','v=2'),('v=1 epoch=1','v=1 epoch=2'),('max=20','max=6'),
                        ('b0=1,2,256,0','b0=1,2,256'),('b0=1,2,256,0','b0=1,2,256,-1'),
                        ('b0=1,2,256,0',f'b0=1,2,256,{2**32}'),('misc=8716425,8716425',f'misc=8716425,{2**64}'),
                        ('max=20','max=20 max=20'),('max=20','unknown=20'),('b0=1,2,256,0','b0=bad')]:
            with self.subTest(old=old):self.assertIsNone(idle_prerequisites(text.replace(old,new,1)))
        self.assertIsNone(idle_prerequisites(self.audit_fixture()))
        self.assertIsNone(idle_prerequisites(text+'cpu-idle-prereq: truncated\n'))
        self.assertIsNone(idle_prerequisites(text.replace('cpu-idle-audit:','missing-audit:')))

    def test_mwait_receipt(self):
        from hardware_cpu_load import mwait_result
        receipt='cpu-mwait: v=1 admitted=1 epoch=1 hint=0 ext=1 cap=32 limit_us=20000 tries=32 returns=32 max_us=10000 fault=0 hlt=32 rechecks=0 tick_begin=20 tick_end=51\n'
        text=self.prereq_fixture()+receipt
        self.assertEqual(mwait_result(text)['status'],'PASS')
        self.assertEqual(load_result(text)['mwait']['status'],'PASS')
        for old,new in [('max_us=10000','max_us=20001'),('fault=0','fault=1'),('hlt=32','hlt=31'),('tick_end=51','tick_end=20'),('max_us=10000','max_us=0')]:
            self.assertEqual(mwait_result(text.replace(old,new))['status'],'FAILED_OR_INCOMPLETE')
        for old,new in [('leaf5_a=64','leaf5_a=128'),('leaf5_b=64','leaf5_b=128'),('hint=0','hint=16'),('ext=1','ext=0'),('cap=32','cap=64'),('limit_us=20000','limit_us=500000'),('tries=32','tries=33'),('returns=32','returns=31'),('admitted=1 epoch=1','admitted=1 epoch=2'),('max_us=10000','max_us=10000 max_us=10000')]:
            self.assertIsNone(mwait_result(text.replace(old,new)))
        self.assertIsNone(mwait_result(text+receipt))
        self.assertIsNone(mwait_result(self.audit_fixture()+receipt))
        self.assertEqual(mwait_result('cpu-mwait: v=1 admitted=0 fallback=hlt\n'),{'status':'refused','fallback':'hlt'})
        self.assertIsNone(mwait_result(self.prereq_fixture()))

    def test_deeper_mwait_receipt(self):
        from hardware_cpu_load import mwait_result
        receipt='cpu-mwait: v=2 admitted=1 epoch=1 hint=16 ext=1 cap=32 limit_us=20000 tries=32 returns=32 max_us=10000 fault=0 hlt=32 rechecks=0 tick_begin=20 tick_end=51\n'
        deep='cpu-mwait-deep: v=1 epoch=1 arat=1 timer=1 tsc=640000000 core_c3=320000000 package_c3=0\n'
        text=self.prereq_fixture()+receipt+deep
        r=mwait_result(text);self.assertEqual(r['status'],'PASS')
        self.assertTrue(r['residency']['core_c3_observed']);self.assertFalse(r['residency']['package_c3_observed'])
        self.assertFalse(mwait_result(text.replace('core_c3=320000000','core_c3=0'))['residency']['core_c3_observed'])
        for old,new in [('max_us=10000','max_us=20001'),('fault=0','fault=3'),('timer=1','timer=0')]:
            r=mwait_result(text.replace(old,new));self.assertEqual(r['status'],'FAILED_OR_INCOMPLETE');self.assertFalse(r['residency']['valid'])
        for old,new in [('arat=1','arat=0'),('hint=16','hint=32'),('core_c3=320000000','core_c3=640000001'),('e2_end=1032','e2_end=1033'),('timer=1','timer=1 timer=1'),('leaf5_d=286531872','leaf5_d=286531616')]:
            self.assertIsNone(mwait_result(text.replace(old,new)),(old,new))
        self.assertIsNone(mwait_result(text+deep));self.assertIsNone(mwait_result(text.replace(deep,'')))
        self.assertEqual(mwait_result('cpu-mwait: v=2 admitted=0 fallback=hlt\n'),{'status':'refused','fallback':'hlt'})

    def test_profile_is_distinct(self):
        dp='ram-usb-v1-mwait-c3-laptop-probe-12345678'
        self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(dp))
        c=boot.limine_config('a'*16,dp)
        self.assertIn('srvros.idle=mwait-c3-test',c);self.assertNotIn('srvros.idle=mwait-wakeup-test',c)
        self.assertIn('srvros.capture=cpu-load',c)
        with self.assertRaises(ValueError):boot.stage(mwait_deep_test=True)
        with self.assertRaises(ValueError):boot.stage(cpu_load_capture=True,mwait_deep_test=True,mwait_wakeup_test=True)
        mw='ram-usb-v1-mwait-laptop-probe-12345678'
        self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(mw))
        self.assertIn('srvros.idle=mwait-wakeup-test',boot.limine_config('a'*16,mw))
        with self.assertRaises(ValueError):boot.stage(mwait_wakeup_test=True)
        v='ram-usb-v35-laptop-probe-12345678'
        self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v))
        c=boot.limine_config('a'*16,v)
        for token in ('srvros.capture=cpu-load','srvros.power=read-only','srvros.smc=read-only'):
            self.assertIn(token,c)
        for token in ('srvros.idle=mwait-wakeup-test','srvros.aml','srvros.capture=desktop-idle','srvros.desktop=trial'):
            self.assertNotIn(token,c)

if __name__=='__main__':unittest.main()
