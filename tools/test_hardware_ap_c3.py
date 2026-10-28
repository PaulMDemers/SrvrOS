import unittest
import hardware_boot as boot
from hardware_ap_c3 import ap_c3_result
import test_hardware_cpu_load as cpu_load_tests
import test_hardware_ap_owner as owner_tests
class CoordinatedTests(unittest.TestCase):
    def fixture(self,zero=False):
        core=0 if zero else 10000000
        s=cpu_load_tests.LoadCaptureTests().prereq_fixture()+owner_tests.APTests().fixture()
        s+='cpu-mwait: v=2 admitted=1 epoch=1 hint=16 ext=1 cap=32 limit_us=20000 tries=32 returns=32 max_us=10000 fault=0 hlt=32 rechecks=0 tick_begin=20 tick_end=51\n'
        s+=f'cpu-mwait-deep: v=1 epoch=1 arat=1 timer=1 tsc=640000000 core_c3={core*32} package_c3=0\n'
        s+='ap-c3: v=1 mode=native epoch=1 rounds=32 fault=0 config=1032 limit_us=20000 completed=yes retained=yes\n'
        for i in range(1,4):s+=f'ap-c3-peer: id={i} mode=1 armed=32 done=32 attempts=32 early=0 config=1032,1032\n'
        for n in range(1,33):s+=f'ap-c3-round: seq={n} us=10020 tsc=20000000 core={core} package=0 p0=1,21000000,{core} p1=2,22000000,{core} p2=3,23000000,{core}\n'
        return s
    def test_observed_and_zero(self):
        r=ap_c3_result(self.fixture());self.assertEqual(r['rounds'],32);self.assertTrue(r['bsp_core_c3_observed']);self.assertFalse(r['package_c3_observed'])
        r=ap_c3_result(self.fixture(True));self.assertFalse(r['bsp_core_c3_observed']);self.assertFalse(any(r['peer_core_c3_observed'].values()))
    def test_corrupt(self):
        for old,new in [('us=10020','us=20001'),('p0=1,','p0=2,'),('p0=1,21000000,10000000','p0=1,21000000,21000001'),('seq=2 ','seq=1 '),('attempts=32','attempts=33'),('early=0','early=1'),('config=1032,1032','config=1032,1024'),('mode=1 armed','mode=2 armed'),('epoch=1 rounds','epoch=2 rounds'),('core_c3=320000000','core_c3=320000001'),('retire_us=3','retire_us=20001'),('state=2 fault=0','state=3 fault=1'),('hlt=32','hlt=31'),('tsc=20000000 core','tsc=0 core'),('us=10020',f'us={2**64}')]:
            with self.subTest(old=old):self.assertIsNone(ap_c3_result(self.fixture().replace(old,new,1)))
        self.assertIsNone(ap_c3_result(self.fixture()+self.fixture()))
        self.assertIsNone(ap_c3_result('\n'.join(self.fixture().splitlines()[:-1])))
    def test_vm_and_truncated_header(self):
        native=self.fixture(True)
        self.assertIsNotNone(ap_c3_result('\n'.join(x for x in native.splitlines() if not x.startswith('ap-owner: v='))))
        self.assertIsNone(ap_c3_result(native.replace('mode=native cpus','mode=vm cpus')))
        vm=native.replace('mode=native','mode=vm-simulation').replace('mode=vm-simulation cpus','mode=vm cpus').replace('mode=1 armed','mode=2 armed').replace('config=1032','config=0').replace(',1032',',0').replace('tsc=20000000 core','tsc=0 core')
        for value in ('21000000','22000000','23000000'):vm=vm.replace(value,'0')
        proof=ap_c3_result(vm);self.assertEqual(proof['mode'],'vm-simulation');self.assertFalse(proof['bsp_core_c3_observed'])
        self.assertIsNone(ap_c3_result(vm.replace('p0=1,0,0','p0=1,1,0',1)))
    def test_profile(self):
        s=boot.limine_config('0123456789abcdef','ram-usb-v1-ap-c3-laptop-probe-6ada85bb')
        for v in ('srvros.capture=cpu-load','srvros.ap=c3-test','srvros.idle=mwait-c3-test'):self.assertIn(v,s)
        self.assertNotIn('srvros.ap-protocol',s);self.assertNotIn('srvros.ap-inject',s)
        for opt in ({},{'laptop_probe_volume':'6ada85bb'},{'laptop_probe_volume':'6ada85bb','cpu_load_capture':True,'ap_ownership_test':True},{'laptop_probe_volume':'6ada85bb','cpu_load_capture':True,'mwait_deep_test':True}):
            with self.assertRaises(ValueError):boot.stage(ap_coordinated_c3=True,**opt)
if __name__=='__main__':unittest.main()
