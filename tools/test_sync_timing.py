import unittest
from sync_timing import sync_timing
class TestSyncTiming(unittest.TestCase):
    def fixture(self):
        lines=['sync-snapshot: version=1 snapshot_us=1000 enabled=1 active=0 invalid=0 dropped=0 final=1',
        'sync-record: version=1 snapshot_us=1000 pid=7 calls=2 total_us=100 files_us=80 devices_us=20 errors=0 conflicts=0 invalid=0 final=1',
        'sync-worst: version=1 snapshot_us=1000 pid=7 start_us=100 files_end_us=140 end_us=150 file_failed=0 device_failed=0 files_usb_failures=0 devices_usb_failures=0 final=1']
        for scope in ('files','devices'):
            for phase in range(3):
                lines.append(f'sync-usb-timing: version=2 scope={scope} id=100 phase={phase} calls=1 callback_us=2 max_us=2 over20ms=0 over100ms=0 over1s=0 observed_calls=1 observe_us=1 resume_us=1 observe_max_us=1 resume_max_us=1 service_checks=1 service_samples=1 service_gap_max_us=1 completion_gap_max_us=1 valid=1')
        return ('\n'.join(lines)+'\n').encode()
    def test_complete(self):self.assertEqual(sync_timing([self.fixture(),self.fixture()])['workers'][0]['phases']['files']['wall_us'],40)
    def test_earlier_incomplete_snapshot(self):
        old=b'\n'.join(line.replace(b'id=100',b'id=0').replace(b'valid=1',b'valid=0') for line in self.fixture().splitlines() if line.startswith(b'sync-usb-timing:'))
        self.assertEqual(sync_timing([old,self.fixture()])['status'],'VALID')
    def test_reject(self):
        for a,b in [('active=0','active=1'),('invalid=0','invalid=1'),('conflicts=0','conflicts=1'),('dropped=0','dropped=1'),('files_us=80','files_us=81'),('files_end_us=140','files_end_us=151'),('calls=2','calls=0'),('file_failed=0','file_failed=1'),('devices_usb_failures=0','devices_usb_failures=1'),('valid=1','valid=0'),('observed_calls=1','observed_calls=0'),('final=1','final=0'),('id=100','id=101')]:
            with self.subTest(a=a):
                with self.assertRaises(ValueError):sync_timing([self.fixture().replace(a.encode(),b.encode())])
        with self.assertRaises(ValueError):sync_timing([b'\n'.join(self.fixture().splitlines()[:-1])])
        with self.assertRaises(ValueError):sync_timing([self.fixture(),self.fixture().replace(b'total_us=100',b'total_us=101')])
if __name__=='__main__':unittest.main()
