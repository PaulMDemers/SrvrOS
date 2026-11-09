import unittest
from capture_write_timing import capture_write_timing,capture_gate_timing
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
class TestCaptureGateTiming(unittest.TestCase):
    def fixture(self):
        rows=[]
        for kind,calls,wall,owned,blocked,ready,running in [(0,2,20,12,10,4,6),(1,1,12,8,6,3,3)]:
            rows.append(f'capture-write-gate: version=1 snapshot_us=1000 pid=42 kind={kind} active=0 calls={calls} wall_us={wall} owned_us={owned} scopes=1 blocked_us={blocked} ready_us={ready} running_us={running} blocks=1 dispatches=1 exclusions=1 invalid=0 final=1')
        rows.append('capture-write-gate-owner: version=1 snapshot_us=1000 pid=42 start_us=110 end_us=122 thread=99 owner_pid=7 syscall=87 scope_start_us=105 scope_end_us=120 overlap_start_us=112 overlap_end_us=120 final=1')
        return ('\n'.join(rows)+'\n').encode()
    def parsed(self):return capture_write_timing([TestCaptureWriteTiming().fixture()],1,71000)
    def test_complete(self):
        self.assertEqual(capture_gate_timing([self.fixture(),self.fixture()],self.parsed())['workers'][0]['owner']['syscall'],87)
    def test_reject(self):
        for a,b in [('active=0','active=1'),('calls=2','calls=3'),('invalid=0','invalid=1'),('blocked_us=10','blocked_us=11'),('owned_us=12','owned_us=21'),('end_us=122','end_us=123'),('thread=99','thread=0'),('scope_start_us=105','scope_start_us=113'),('overlap_end_us=120','overlap_end_us=123'),('pid=42','pid=43'),('dispatches=1','dispatches=0'),('kind=1','kind=2'),('final=1','final=0')]:
            with self.subTest(a=a):
                with self.assertRaises(ValueError):capture_gate_timing([self.fixture().replace(a.encode(),b.encode())],self.parsed())
        with self.assertRaises(ValueError):capture_gate_timing([self.fixture(),self.fixture().replace(b'owner_pid=7',b'owner_pid=8')],self.parsed())
        with self.assertRaises(ValueError):capture_gate_timing([b''],self.parsed())
        with self.assertRaises(ValueError):capture_gate_timing([b'\n'.join(self.fixture().splitlines()[:-1])],self.parsed())
if __name__=='__main__':unittest.main()
