import unittest
from lifetime_timing import lifetime_worst_records

def fixture():
    clock='pid=42 cycle=5 begin_us=100 launch_begin_us=110 launch_end_us=120 ready_us=200 close_request_us=300 visual_us=310 exit_seen_us=340 reap_queued_us=350 reap_begin_us=360 reap_end_us=390 closed_us=400'
    return ('\n'.join(f'desktop-life-worst: version=1 snapshot_us=500 kind={kind} {clock} final=1' for kind in ['ready','close'])+'\n').encode()
class Lifetime(unittest.TestCase):
    def test_retained_pair_and_dedup(self):
        r=lifetime_worst_records([fixture(),fixture()],100,100);self.assertEqual(r['close']['pid'],42);self.assertEqual(r['ready']['cycle'],5)
    def test_absent_unobserved_fast_reap(self):
        raw=fixture()
        for key in ['exit_seen_us=340','reap_queued_us=350','reap_begin_us=360','reap_end_us=390']:
            raw=raw.replace(key.encode(),key.split('=')[0].encode()+b'=0')
        self.assertEqual(lifetime_worst_records([raw],100,100)['close']['reap_end_us'],0)
    def test_missing_conflicting_clock_and_maximum(self):
        for raw in [b'',fixture().replace(b'final=1',b'final=0'),fixture().replace(b'launch_end_us=120',b'launch_end_us=250'),fixture().replace(b'visual_us=310',b'visual_us=250'),fixture().replace(b'reap_end_us=390',b'reap_end_us=410'),fixture().replace(b'reap_begin_us=360',b'reap_begin_us=0')]:
            with self.subTest(raw=raw[:50]),self.assertRaises(AssertionError):lifetime_worst_records([raw],100,100)
        with self.assertRaises(AssertionError):lifetime_worst_records([fixture()],101,100)
        with self.assertRaises(AssertionError):lifetime_worst_records([fixture(),fixture().replace(b'pid=42',b'pid=43')],100,100)
if __name__=='__main__':unittest.main()
