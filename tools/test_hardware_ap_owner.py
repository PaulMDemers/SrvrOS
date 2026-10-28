import unittest
from hardware_ap_owner import ap_owner_result
import hardware_boot as boot
class APTests(unittest.TestCase):
    def fixture(self):
        s='ap-owner: v=1 admitted=1 mode=native cpus=4 bsp=0 mask=15 method=monitor-c1 rounds=32 limit_us=20000 retained=yes\n'
        for i in range(1,4):s+=f'ap-owner-cpu: id={i} seen={i} signature=263761 ready_us=100 rounds=32 max_us=2 retire_us=3 state=2 fault=0 waits=32\n'
        return s+'ap-owner: completed=yes retired=yes memory=retained policy_writes=0\n'
    def test_good(self):
        self.assertEqual(ap_owner_result(self.fixture())['cpus'],4)
    def test_bad(self):
        for a,b in [('cpus=4','cpus=5'),('mask=15','mask=7'),('seen=1','seen=2'),('signature=263761','signature=263760'),('rounds=32 max','rounds=31 max'),('ready_us=100','ready_us=20001'),('max_us=2 ','max_us=20001 '),('retire_us=3','retire_us=20001'),('state=2','state=1'),('fault=0','fault=1'),('waits=32','waits=0'),('completed=yes','completed=NO'),('policy_writes=0','policy_writes=1'),('method=monitor-c1','method=pause')]:
            with self.subTest(a=a):self.assertIsNone(ap_owner_result(self.fixture().replace(a,b)))
        self.assertIsNone(ap_owner_result(self.fixture()+self.fixture()))
        self.assertIsNone(ap_owner_result(self.fixture().replace('id=2 seen=2','id=1 seen=1')))
        self.assertIsNone(ap_owner_result('\n'.join(self.fixture().splitlines()[1:])))
    def test_stage(self):
        v='ram-usb-v1-ap-owner-laptop-probe-6ada85bb-auto-return'
        s=boot.limine_config('0123456789abcdef',v)
        self.assertIn('srvros.capture=ap-ownership',s)
        self.assertIn('srvros.capture-failure=reboot',s)
        for word in ('srvros.battery','srvros.aml','srvros.idle=','srvros.audio','srvros.power='):self.assertNotIn(word,s)
        for opts in ({},{'laptop_probe_volume':'6ada85bb','mwait_deep_test':True},{'laptop_probe_volume':'6ada85bb','cpu_load_capture':True},{'laptop_probe_volume':'6ada85bb','desktop_basic':True}):
            with self.assertRaises(ValueError):boot.stage(ap_ownership_test=True,**opts)
if __name__=='__main__':unittest.main()
