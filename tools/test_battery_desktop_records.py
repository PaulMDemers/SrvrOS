import unittest
from battery_desktop_smoke import check_capture
from test_sci_observe_records import BASE as SCI
from hardware_desktop import battery_desktop_result, needs_attention

BASE = SCI + b'''battery-desktop: started pid=4 epoch=2 generation=1 start_us=1000 storage=ram
desktop-coexist: completed=yes start_us=2000 end_us=60002000 loops=6000 max_gap_us=12000 generated=60 presented=60 frames=61 storage=ram
deskclient: exited frames=61 input=Ab
deskd: exited frames=65 fb_bytes=1000000
battery-service: retired epoch=2 attempts=24 rejected=0 queries=100 notifications=100 attached=1 detached=1 heap=0 errors=0 denied=0
battery-desktop: PASS outcome=complete epoch=2 first_gen=1 last_gen=24 start_us=1000 end_us=60500000 reads=6100 busy_reads=600 process_status=0; retired=yes
'''

class CoexistRecords(unittest.TestCase):
    def test_complete(self):
        self.assertEqual(check_capture(BASE)['presented'],60)

    def test_missing_duplicate_or_failure(self):
        for prefix in (b'battery-desktop: started',b'battery-desktop: PASS',b'desktop-coexist: completed',b'battery-service: retired'):
            line=next(l for l in BASE.splitlines(keepends=True) if l.startswith(prefix))
            with self.subTest(prefix=prefix):
                with self.assertRaises(AssertionError):check_capture(BASE.replace(line,b''))
        for prefix in (b'battery-desktop: started',b'battery-desktop: PASS',b'desktop-coexist: completed'):
            line=next(l for l in BASE.splitlines(keepends=True) if l.startswith(prefix))
            with self.assertRaises(AssertionError):check_capture(BASE+line)
        for failure in (b'deskclient: FAIL',b'deskd: FAIL',b'battery-desktop: FAIL',b'battery: HALTED'):
            with self.assertRaises(AssertionError):check_capture(BASE+failure+b'\n')

    def test_collector_requires_recipe_and_return(self):
        prefix=b'laptop-capture: boot options=srvros.capture=battery-desktop\n'
        suffix=b'battery-desktop-capture: returned=yes clean=yes; physical accuracy NOT_TESTED\n'
        self.assertIsNotNone(battery_desktop_result((prefix+BASE+suffix).decode()))
        for data in (BASE,prefix+BASE,BASE+suffix,prefix+BASE+suffix.replace(b'clean=yes',b'clean=NO')):
            self.assertIsNone(battery_desktop_result(data.decode()))
        for line in (prefix+BASE+suffix).decode().splitlines():self.assertFalse(needs_attention(line),line)
        self.assertTrue(needs_attention('battery-desktop: FAIL outcome=desktop deadline'))
        self.assertTrue(needs_attention('desktop-coexist: completed=NO'))

    def test_no_false_overlap_or_progress(self):
        changes=((b'first_gen=1',b'first_gen=2'),(b'last_gen=24',b'last_gen=1'),
                 (b'end_us=60500000',b'end_us=60001000'),(b'end_us=60002000',b'end_us=50002000'),
                 (b'start_us=2000',b'start_us=1'),(b'busy_reads=600',b'busy_reads=0'),
                 (b'presented=60',b'presented=59'),(b'frames=61',b'frames=1'),
                 (b'process_status=0',b'process_status=1'),(b'detached=1',b'detached=0'),
                 (b'heap=0',b'heap=1'),(b'epoch=2 first_gen',b'epoch=3 first_gen'))
        for old,new in changes:
            with self.subTest(old=old,new=new):
                with self.assertRaises(AssertionError):check_capture(BASE.replace(old,new))

if __name__=='__main__':unittest.main()
