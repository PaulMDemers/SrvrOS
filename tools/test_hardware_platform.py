import unittest
import hardware_boot as boot
from hardware_platform import platform_result,latency_decode,platform_timing_result,platform_series_result,platform_display_result,platform_resources_result,RESOURCE_OFFSETS
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
    def timed_fixture(self):
        return self.fixture().replace('ap-platform: v=1','ap-platform: v=2') + (
            'ap-platform-time: phase=0 pm_us=3,4 tsc=10000,10100 nominal_mhz=1400 reference=acpi-pm\n'
            'ap-platform-time: phase=1 pm_us=92000003,92000004 tsc=128800010000,128800010100 nominal_mhz=1400 reference=acpi-pm\n')
    def test_timing(self):
        s=self.timed_fixture();r=platform_result(s)
        self.assertEqual(r['timing']['pm_elapsed_us_bounds'],[91999999,92000001])
        self.assertTrue(r['timing']['original_bound_ok'])
        # Actual failed native raw numbers remain a failure. Brackets are
        # deliberately synthetic; this does not pretend to reconstruct them.
        s=s.replace('time_us=2,92000002','time_us=2092964,92151507').replace('rc6=100,1000','rc6=13525192,83959518')
        s=s.replace('pm_us=3,4','pm_us=2092965,2092966').replace('pm_us=92000003,92000004','pm_us=92151508,92151509')
        t=platform_timing_result(s);self.assertFalse(t['original_bound_ok']);self.assertFalse(t['acceptance'])
        self.assertEqual(t['original_excess_ns'],97393000)
        self.assertGreater(t['bracket_excess_ns'],97000000)
        self.assertIsNone(platform_result(s))
        self.assertIsNotNone(platform_timing_result(s.replace('fault=0 devices','fault=1 devices')))
    def test_timing_refusals(self):
        s=self.timed_fixture()
        for old,new in [('phase=1','phase=0'),('pm_us=3,4','pm_us=4,3'),('pm_us=3,4','pm_us=1,4'),('tsc=10000,10100','tsc=10100,10000'),('nominal_mhz=1400 reference','nominal_mhz=0 reference'),('reference=acpi-pm','reference=unknown')]:
            with self.subTest(old=old):self.assertIsNone(platform_result(s.replace(old,new)))
        self.assertIsNone(platform_result('\n'.join(s.splitlines()[:-1])))
        self.assertIsNone(platform_result(s+s.splitlines()[-1]+'\n'))
        # Legacy receipts retain exactly the old strict cross-clock predicate.
        self.assertIsNone(platform_result(self.fixture().replace('rc6=100,1000','rc6=100,71900000')))
    def series_fixture(self):
        s=self.timed_fixture().replace('ap-platform: v=2','ap-platform: v=3')
        s+='ap-platform-series: v=1 count=5 cap=12 checkpoints=9 stride=2 overflow=0 completed=yes\n'
        for seq in range(1,6):
            us=seq*20000000-15000000;ts=us*1400
            s+=f'ap-platform-point: seq={seq} sample_us={us} pm_us={us+1},{us+2} tsc={ts},{ts+100} rc6={seq*100},{seq*100+1} status=3,3\n'
        return s
    def test_series(self):
        s=self.series_fixture();r=platform_series_result(s)
        self.assertTrue(r['continuity_ok']);self.assertFalse(r['acceptance'])
        self.assertEqual(len(r['intervals']),6)
        self.assertIsNotNone(platform_result(s))
        # A burst/reset is preserved as evidence, never accepted as continuity.
        bad=s.replace('rc6=400,401','rc6=50,51')
        self.assertFalse(platform_series_result(bad)['continuity_ok']);self.assertIsNone(platform_result(bad))
        # Sustained excess remains observable with the original bound failing.
        bad=s.replace('rc6=100,1000','rc6=100,72000000')
        self.assertFalse(platform_series_result(bad)['original_bound_ok']);self.assertIsNone(platform_result(bad))
        # One counter wrap is handled consistently across endpoint and series.
        wrapped=s.replace('rc6=100,1000','rc6=4294967200,1000')
        self.assertTrue(platform_series_result(wrapped)['continuity_ok']);self.assertIsNotNone(platform_result(wrapped))
        zero=s.replace('rc6=100,1000','rc6=100,100')
        for seq in range(1,10):zero=zero.replace(f'rc6={seq*100},{seq*100+1}','rc6=100,100')
        self.assertTrue(platform_series_result(zero)['continuity_ok']);self.assertIsNotNone(platform_result(zero))
    def test_series_refusals(self):
        s=self.series_fixture()
        for old,new in [('count=5 cap=12 checkpoints=9 stride=2','count=13 cap=12 checkpoints=25 stride=2'),('retained=9','retained=8'),('overflow=0 completed','overflow=1 completed'),('seq=2 sample','seq=1 sample'),('sample_us=65000000 pm','sample_us=65000001 pm'),('pm_us=65000001,65000002','pm_us=65000002,65000001'),('rc6=400,401','rc6=400,4294967295'),('status=3,3','status=3,4294967295')]:
            with self.subTest(old=old):self.assertIsNone(platform_series_result(s.replace(old,new)))
        self.assertIsNone(platform_result('\n'.join(s.splitlines()[:-1])))
        self.assertIsNone(platform_result(s+s.splitlines()[-1]+'\n'))
    def test_profile(self):
        c=boot.limine_config('0123456789abcdef','ram-usb-v1-ap-platform-laptop-probe-6ada85bb')
        for t in ['srvros.ap-platform=read-only','srvros.ap-package=read-only','srvros.ap=c3-test','srvros.capture=cpu-load']:self.assertIn(t,c)
        self.assertNotIn('ap-platform',boot.limine_config('0123456789abcdef','ram-usb-v1-ap-package-laptop-probe-6ada85bb'))
        with self.assertRaises(ValueError):boot.stage(ap_platform_audit=True)
    def display_fixture(self):
        return self.series_fixture().replace('ap-platform: v=3','ap-platform: v=4') + (
            'ap-platform-well: phase=0 pm_us=5,6 ctl=3221225472,1073741824,0,0\n'
            'ap-platform-well: phase=1 pm_us=92000005,92000006 ctl=0,0,0,0\n')
    def test_display_observation_and_rc6_failure(self):
        s=self.display_fixture();r=platform_display_result(s)
        self.assertFalse(r['acceptance']);self.assertTrue(r['samples'][0]['controls']['bios']['request'])
        self.assertTrue(r['samples'][0]['controls']['driver']['state'])
        self.assertFalse(r['samples'][0]['controls']['driver']['request'])
        self.assertFalse(r['samples'][1]['controls']['bios']['state'])
        self.assertIsNotNone(platform_result(s))
        bad=s.replace('rc6=100,1000','rc6=100,72000000')
        self.assertIsNotNone(platform_display_result(bad));self.assertIsNone(platform_result(bad))
        self.assertIsNotNone(platform_display_result(s.replace('fault=0 devices','fault=1 devices').replace('completed=yes','completed=NO')))
    def test_display_refusal(self):
        s=self.display_fixture()
        for old,new in [('phase=1 pm_us=92000005','phase=0 pm_us=92000005'),('pm_us=5,6','pm_us=6,5'),('pm_us=5,6','pm_us=3,6'),('pm_us=5,6','pm_us=5,92000003'),('ctl=0,0,0,0','ctl=0,0,0,4294967295'),('v=4 fault','v=3 fault'),('base=3221225472','base=0')]:
            with self.subTest(old=old):self.assertIsNone(platform_display_result(s.replace(old,new)))
        self.assertIsNone(platform_display_result('\n'.join(s.splitlines()[:-1])))
        self.assertIsNone(platform_result(s+s.splitlines()[-1]+'\n'))
    def resource_fixture(self):
        s=self.display_fixture().replace('ap-platform: v=4','ap-platform: v=5')
        raw=[0]*26
        raw[0]=0xc0000000;raw[7]=0x82004000;raw[8]=1<<31;raw[14]=0x27;raw[20]=1<<31;raw[25]=1<<31
        for phase,us in [(0,7),(1,92000007)]:
            s+=f'ap-platform-resources: v=1 phase={phase} state=1 pm_us={us},{us+1} reads=26 count=26 before=3221225472,1073741824,1073741824,1073741824 after=3221225472,1073741824,1073741824,1073741824\n'
            s+=f'ap-platform-resource: phase={phase} count=26 raw='+','.join(map(str,raw))+'\n'
        return s
    def test_resource_controls(self):
        s=self.resource_fixture();r=platform_resources_result(s)
        self.assertEqual(r['status'],'OBSERVED');self.assertFalse(r['acceptance'])
        v=r['samples'][0]['decoded'];self.assertEqual(v['edp_input_select'],4)
        self.assertTrue(v['transcoders'][0]['state']);self.assertTrue(v['primary_enabled'][0]);self.assertTrue(v['vga_disabled'])
        self.assertIsNotNone(platform_result(s));self.assertTrue(platform_result(s)['resources'])
        # Unchanged original timing failure is visible independently, never accepted.
        bad=s.replace('rc6=100,1000','rc6=100,72000000')
        self.assertEqual(platform_resources_result(bad)['status'],'OBSERVED');self.assertIsNone(platform_result(bad))
        # Source failure stays source failure even if the raw inventory is complete.
        bad=s.replace('fault=0 devices','fault=1 devices').replace('completed=yes','completed=NO')
        self.assertFalse(platform_resources_result(bad)['source_completed']);self.assertIsNone(platform_result(bad))
        # Off-domain must be unknown with no dependent reads, not disabled controls.
        off=s.replace('phase=0 state=1 pm_us=7,8 reads=26','phase=0 state=2 pm_us=7,8 reads=0')
        off=off.replace('before=3221225472,1073741824,1073741824,1073741824 after=3221225472,1073741824,1073741824,1073741824','before=0,0,0,0 after=0,0,0,0',1)
        import re
        off=re.sub(r'ap-platform-resource: phase=0 count=26 raw=[0-9,]+','ap-platform-resource: phase=0 count=26 raw='+','.join(['0']*26),off)
        self.assertEqual(platform_resources_result(off)['status'],'UNAVAILABLE')
        self.assertIsNone(platform_resources_result(off)['samples'][0]['decoded']);self.assertIsNone(platform_result(off))
    def test_resource_refusals(self):
        s=self.resource_fixture()
        for old,new in [('phase=1 state=1','phase=0 state=1'),('pm_us=7,8','pm_us=8,7'),('pm_us=7,8','pm_us=4,8'),('pm_us=7,8','pm_us=7,92000007'),('phase=1 count=26 raw=','phase=0 count=26 raw='),('count=26 raw=','count=25 raw='),('raw=3221225472','raw=4294967295'),('reads=26','reads=25'),('state=1 pm_us=7','state=3 pm_us=7'),('after=3221225472','after=1073741824'),('before=3221225472','before=0'),('v=5 fault','v=4 fault')]:
            with self.subTest(old=old):self.assertIsNone(platform_resources_result(s.replace(old,new)))
        self.assertIsNone(platform_resources_result('\n'.join(s.splitlines()[:-1])))
        self.assertIsNone(platform_resources_result(s+s.splitlines()[-1]+'\n'))
        # Compare the whole independently defined allowlist against the C producer.
        import re
        from pathlib import Path
        h=Path('kernel/include/srvros/display_resource_audit.h').read_text()
        values=re.search(r'display_resource_offsets\[DISPLAY_RESOURCE_COUNT\]=\{([^}]+)',h)[1]
        self.assertEqual(tuple(int(x,16) for x in re.findall(r'0x[0-9a-f]+',values)),RESOURCE_OFFSETS)
if __name__=='__main__':unittest.main()
