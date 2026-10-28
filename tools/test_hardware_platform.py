import unittest
import hardware_boot as boot
from hardware_platform import platform_result,latency_decode
from test_hardware_package import PackageTests
class PlatformTests(unittest.TestCase):
    def fixture(self):
        s=PackageTests().fixture().replace('bdf=160 id=2620481670 class=787248','bdf=16 id=170295430 class=196608')
        s+='ap-platform: v=1 fault=0 devices=1 policy_writes=0 forcewake_writes=0 permission=unavailable reason=no-validated-grant-register completed=yes\n'
        s+='ap-platform-gfx: bdf=16 id=170295430 base=3221225472 time_us=2,92000002 status=0,3 rc6=100,1000 config=1032,1032 rc_control=unavailable\n'
        s+='ap-platform-ltr: bdf=16 id=170295430 state=1,1 offset=256,256 raw=134285313,134285313 headers=1,1\n'
        return s
    def test_valid_and_unavailable(self):
        r=platform_result(self.fixture());self.assertTrue(r['graphics']['rc6_observed']);self.assertEqual(r['permission'],'unavailable')
        self.assertFalse(platform_result(self.fixture().replace('rc6=100,1000','rc6=100,100'))['graphics']['rc6_observed'])
        self.assertEqual(platform_result(self.fixture().replace('rc6=100,1000','rc6=4294967290,10'))['graphics']['rc6_delta_ticks'],16)
        self.assertIsNotNone(platform_result(self.fixture().replace('state=1,1 offset=256,256 raw=134285313,134285313','state=2,2 offset=0,0 raw=0,0')))
        self.assertEqual(latency_decode(0x0801),1024);self.assertEqual(latency_decode(0),0)
        for v in [0x8000,0x1801,0xe000]:self.assertIsNone(latency_decode(v))
    def test_reject(self):
        for old,new in [('forcewake_writes=0','forcewake_writes=1'),('fault=0 devices','fault=1 devices'),('permission=unavailable','permission=granted'),('rc6=100,1000','rc6=100,4294967295'),('rc6=100,1000','rc6=100,2000000000'),('config=1032,1032','config=1032,1033'),('state=1,1 offset','state=3,3 offset'),('offset=256,256','offset=4092,4092'),('headers=1,1','headers=961,961'),('base=3221225472','base=3221225488'),('time_us=2,92000002','time_us=2,92000000')]:
            with self.subTest(old=old):self.assertIsNone(platform_result(self.fixture().replace(old,new)))
        self.assertIsNone(platform_result(self.fixture()+self.fixture().splitlines()[-1]+'\n'))
        self.assertIsNone(platform_result('\n'.join(self.fixture().splitlines()[:-1])))
    def test_profile(self):
        c=boot.limine_config('0123456789abcdef','ram-usb-v1-ap-platform-laptop-probe-6ada85bb')
        for t in ['srvros.ap-platform=read-only','srvros.ap-package=read-only','srvros.ap=c3-test','srvros.capture=cpu-load']:self.assertIn(t,c)
        self.assertNotIn('ap-platform',boot.limine_config('0123456789abcdef','ram-usb-v1-ap-package-laptop-probe-6ada85bb'))
        with self.assertRaises(ValueError):boot.stage(ap_platform_audit=True)
if __name__=='__main__':unittest.main()
