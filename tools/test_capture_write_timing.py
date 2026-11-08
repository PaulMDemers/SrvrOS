import unittest
from capture_write_timing import capture_write_timing
class TestCaptureWriteTiming(unittest.TestCase):
    def fixture(self):
        lines=['capture-write-snapshot: version=1 snapshot_us=1000 enabled=1 invalid=0 dropped=0 final=1',
               'capture-write-record: version=1 snapshot_us=1000 pid=42 first_us=100 last_us=900 retired=1 attempts=2 calls=2 requested=71000 bytes=71000 failures=0 growths=1 invalid=0 final=1']
        for site,us in enumerate([100,20,40,30,10,5]):lines.append(f'capture-write-sample: version=1 snapshot_us=1000 pid=42 site={site} count=2 total_us={us} max_us={us} final=1')
        return ('\n'.join(lines)+'\n').encode()
    def test_complete(self):
        self.assertEqual(capture_write_timing([self.fixture(),self.fixture()],1,71000)['workers'][0]['sites']['handler']['total_us'],40)
    def test_failures(self):
        for a,b in [('dropped=0','dropped=1'),('attempts=2','attempts=3'),('retired=1','retired=0'),('bytes=71000','bytes=70000'),('invalid=0','invalid=1'),('final=1','final=0'),('total_us=40','total_us=1'),('site=5','site=6'),('last_us=900','last_us=101')]:
            with self.subTest(a=a):
                with self.assertRaises(ValueError):capture_write_timing([self.fixture().replace(a.encode(),b.encode())],1,71000)
        with self.assertRaises(ValueError):capture_write_timing([b'\n'.join(self.fixture().splitlines()[:-1])],1,71000)
        with self.assertRaises(ValueError):capture_write_timing([self.fixture(),self.fixture().replace(b'calls=2',b'calls=3')],1,71000)
if __name__=='__main__':unittest.main()
