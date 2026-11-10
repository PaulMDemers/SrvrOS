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
class TestReservedWritePartition(unittest.TestCase):
    def fixture(self,all_fast=False):
        text=TestCaptureWriteTiming().fixture().decode().replace('version=1','version=2')
        fast='fast_calls=2 fast_requested=71000 fast_bytes=71000 fast_failures=0 fast_max_request=65536 fast_max_bytes=65536' if all_fast else 'fast_calls=1 fast_requested=65536 fast_bytes=65536 fast_failures=0 fast_max_request=65536 fast_max_bytes=65536'
        text=text.replace('invalid=0 final=1\n','invalid=0 '+fast+' final=1\n',1)
        # Only the parent record has that suffix in this fixture, not header.
        for site in (1,3):
            old=next(line for line in text.splitlines() if f'site={site} ' in line)
            new=old.replace('count=2','count=0' if all_fast else 'count=1')
            if all_fast:new=new.replace('total_us=20 max_us=20','total_us=0 max_us=0').replace('total_us=30 max_us=30','total_us=0 max_us=0')
            text=text.replace(old,new)
        if all_fast:text=text.replace('growths=1','growths=0')
        return text.encode()
    def gates(self,all_fast=False):
        if not all_fast:return TestCaptureGateTiming().fixture().replace(b'kind=0 active=0 calls=2',b'kind=0 active=0 calls=1')
        fields='wall_us owned_us scopes blocked_us ready_us running_us blocks dispatches exclusions invalid'.split()
        rows=[f'capture-write-gate: version=1 snapshot_us=1000 pid=42 kind={kind} active=0 calls=0 '+' '.join(k+'=0' for k in fields)+' final=1' for kind in (0,1)]
        names='start_us end_us thread owner_pid syscall scope_start_us scope_end_us overlap_start_us overlap_end_us'.split()
        rows.append('capture-write-gate-owner: version=1 snapshot_us=1000 pid=42 '+' '.join(k+'=0' for k in names)+' final=1')
        return ('\n'.join(rows)+'\n').encode()
    def test_mixed_partition_is_explicit(self):
        observed=capture_write_timing([self.fixture()],1,71000)
        self.assertEqual((observed['version'],observed['workers'][0]['ordinary_calls']),(2,1))
        self.assertEqual(capture_gate_timing([self.gates()],observed)['workers'][0]['total']['calls'],1)
    def test_private_only_requires_explicit_zero_ordinary_evidence(self):
        observed=capture_write_timing([self.fixture(True)],1,71000)
        self.assertEqual(observed['workers'][0]['ordinary_calls'],0)
        self.assertEqual(capture_gate_timing([self.gates(True)],observed)['workers'][0]['worst']['calls'],0)
        for broken in (b'',self.gates(True).replace(b'thread=0',b'thread=1'),self.gates(True).replace(b'wall_us=0',b'wall_us=1')):
            with self.assertRaises(ValueError):capture_gate_timing([broken],observed)
    def test_private_partition_cannot_hide_work_or_enlarge_bounds(self):
        for a,b in [(b'fast_calls=1',b'fast_calls=3'),(b'fast_failures=0',b'fast_failures=1'),
                    (b'fast_bytes=65536',b'fast_bytes=65535'),(b'fast_requested=65536',b'fast_requested=65537'),
                    (b'fast_max_request=65536',b'fast_max_request=65537'),(b'fast_max_bytes=65536',b'fast_max_bytes=1'),
                    (b'fast_calls=1',b'fast_calls=0'),(b'fast_calls=1',b'fast_calls=18446744073709551616'),
                    (b'fast_max_bytes=65536 ',b''),(b'count=1',b'count=2'),(b'version=2',b'version=3')]:
            with self.subTest(a=a),self.assertRaises(ValueError):capture_write_timing([self.fixture().replace(a,b)],1,71000)
    def test_all_fast_cannot_claim_growth_or_missing_phase(self):
        for value in (self.fixture(True).replace(b'growths=0',b'growths=1'),self.fixture(True).replace(b'count=0 total_us=0',b'count=0 total_us=1')):
            with self.assertRaises(ValueError):capture_write_timing([value],1,71000)
    def test_legacy_remains_strict(self):
        with self.assertRaises(ValueError):capture_write_timing([self.fixture().replace(b'version=2',b'version=1')],1,71000)
        with self.assertRaises(ValueError):capture_gate_timing([self.gates()],TestCaptureGateTiming().parsed())

if __name__=='__main__':unittest.main()
