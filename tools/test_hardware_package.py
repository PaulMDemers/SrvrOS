import unittest
import hardware_boot as boot
from hardware_package import package_result,irtl_decode
import test_hardware_ap_c3 as coord_tests
class PackageTests(unittest.TestCase):
    def fixture(self):
        s=coord_tests.CoordinatedTests().fixture()
        s+='ap-package: v=1 epoch=1 rounds=32 fault=0 irtl=34882,34882 snap_us=1,92000001 devices=1 policy_writes=0 gpu_rc6=unknown platform_permission=unknown completed=yes\n'
        for n in range(1,33):s+=f'ap-package-round: seq={n} tsc=20050000 c2=0\n'
        for phase in (0,1):s+=f'ap-package-pci: phase={phase} bdf=160 id=2620481670 class=787248 state=1 command=6 flags=7 pm=0 link_cap=3072 link=3 ltr_cap=2048 ltr=1024\n'
        return s
    def test_valid_zero_and_decode(self):
        r=package_result(self.fixture());self.assertEqual(r['irtl']['limit_ns'],67584);self.assertFalse(r['package_c2_observed'])
        self.assertTrue(r['devices'][0]['decoded_start']['ltr_enabled'])
        self.assertTrue(package_result(self.fixture().replace('c2=0','c2=1000'))['package_c2_observed'])
        for raw in (0,0x8000|6<<10,0xa800,2**64):self.assertIsNone(irtl_decode(raw))
        self.assertEqual(irtl_decode(0x8001)['limit_ns'],1)
        self.assertEqual(irtl_decode(0x8001|(5<<10))['limit_ns'],33554432)
    def test_reject_corruption(self):
        for a,b in [('epoch=1 rounds=32 fault=0 irtl','epoch=2 rounds=32 fault=0 irtl'),('irtl=34882,34882','irtl=34882,34883'),('tsc=20050000','tsc=19999999'),('c2=0','c2=20050001'),('seq=2 tsc=20050000','seq=1 tsc=20050000'),('state=1 command','state=2 command'),('devices=1','devices=2'),('phase=1','phase=0'),('flags=7','flags=0'),('policy_writes=0 gpu','policy_writes=1 gpu'),('hlt=32','hlt=31')]:
            with self.subTest(a=a):self.assertIsNone(package_result(self.fixture().replace(a,b,1)))
        self.assertIsNone(package_result(self.fixture()+self.fixture()))
        self.assertIsNone(package_result('\n'.join(self.fixture().splitlines()[:-1])))
        self.assertIsNone(package_result('ap-package: v=1 unavailable=vm policy_writes=0\n'))
    def test_profile(self):
        c=boot.limine_config('0123456789abcdef','ram-usb-v1-ap-package-laptop-probe-6ada85bb')
        for token in ('srvros.ap-package=read-only','srvros.ap=c3-test','srvros.capture=cpu-load','srvros.idle=mwait-c3-test'):self.assertIn(token,c)
        self.assertNotIn('srvros.ap-package',boot.limine_config('0123456789abcdef','ram-usb-v1-ap-c3-laptop-probe-6ada85bb'))
        with self.assertRaises(ValueError):boot.stage(ap_package_audit=True)
        with self.assertRaises(ValueError):boot.stage(ap_package_audit=True,ap_coordinated_c3=True,laptop_probe_volume='6ada85bb')
if __name__=='__main__':unittest.main()
