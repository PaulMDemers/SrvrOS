import unittest
from battery_home_desktop_records import check_capture


def fixture():
    return b'''workspace-log: snapshot=boot.log monotonic_us=100 total_before=0 tail_limit=65536
battery-home-desktop-capture: begin seconds=180 watchdog_seconds=240 startup=disabled input=none
battery-workspace-hold: acquired token=1
battery-workspace-controls: begin mounted=yes native=explicit
battery-home-monitor: begin continuous=yes log_interval_us=5000000 native=explicit-capture
battery-home-desktop: logging=started apps=ordinary native=explicit-capture
battery-desktop-lifetime: acquired token=1 root=5
battery-monitor: auto_start=1 native=explicit-capture
battery-workspace-hold: guarded token=1 unmount=blocked scratch=blocked image=blocked
workspace-log: snapshot=session-a.log monotonic_us=11000000 total_before=1000 tail_limit=65536
workspace-log: snapshot=session-b.log monotonic_us=21000000 total_before=2000 tail_limit=65536
battery-monitor-events: epoch=1 at_us=5000000 events=32 alarms=0 acknowledgements=0 unknown=0 invalidations=0 saturated=0 final=0
desktop-idle: elapsed_us=180000000 sampled_ticks=18000 idle_ticks=9000 switches=100 preemptions=50 frames=30 fb_bytes=4000 measured=yes
desktop-idle: completed=yes screenshot=yes
battery-home-desktop-sample: first_gen=1 last_gen=66 elapsed_us=190000000 complete=yes
battery-desktop-lifetime: drained token=1 members=0 peak=4 revoked=yes
battery-home-return: idle=1 held=1; sources_masked=1
battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0
battery-monitor-events: epoch=1 at_us=190000000 events=2100 alarms=0 acknowledgements=0 unknown=0 invalidations=0 saturated=0 final=1
battery-controls-events: epoch=1 events=2100 alarms=0 acknowledgements=0 unknown=0 invalid_through=0 generation=66; drained=yes
battery: foreign-event restored enable=0x62f original=0x40 protected_writes=1
battery: foreign-event restored enable=0x62e original=0x4 protected_writes=1
battery-service: retired epoch=1 attempts=66 rejected=0 queries=2100 notifications=1 attached=1 detached=1 heap=0 errors=0 denied=0
battery-desktop-lifetime: released token=1 runtime_retired=yes
battery-controls: PASS requests=0 descendants=4 epoch=1; revoked=yes retired=yes
battery-home-desktop: logger_retired=yes final_deferred=yes
battery-workspace-controls: complete=yes epoch=1 state=4; revoked=yes retired=yes mounted=yes native=explicit
battery-workspace-hold: released token=1
battery-home-desktop-capture: returned=yes clean=yes; physical accuracy NOT_TESTED
laptop-capture: completed=yes
workspace-log: snapshot=exit.log monotonic_us=191000000 total_before=9000 tail_limit=65536
'''


class Records(unittest.TestCase):
    def test_complete_vm_and_native(self):
        data=fixture();self.assertEqual(check_capture(data,vm=True)['battery_events'],2100)
        self.assertEqual(check_capture(data.replace(b'\n',b'\r\n'),vm=True)['peak_members'],4)
        native=data.replace(b'0x62f',b'0x189c').replace(b'0x62e',b'0x189e').replace(b'attached=1 detached=1',b'attached=2 detached=2').replace(b'queries=2100',b'queries=2104')
        check_capture(native)
        with self.assertRaises(AssertionError):check_capture(native,vm=True)
        with self.assertRaises(AssertionError):check_capture(data)

    def test_missing_duplicate_torn(self):
        data=fixture()
        # Every completion/ownership receipt is required exactly once.
        for row in data.splitlines(keepends=True):
            if row.startswith((b'battery-home-desktop',b'battery-desktop-lifetime',b'battery-workspace-',b'battery-service:',b'desktop-idle:',b'laptop-capture:')):
                for bad in (data.replace(row,b'',1),data+row,data.replace(row,row[:-2]+b'\n',1)):
                    with self.subTest(row=row),self.assertRaises(AssertionError):check_capture(bad,vm=True)
        with self.assertRaises(AssertionError):check_capture(data[:-1],vm=True)

    def test_insufficient_samples_duration_and_unsafe_retirement(self):
        data=fixture()
        for old,new in ((b'first_gen=1',b'first_gen=0'),(b'last_gen=66',b'last_gen=20'),
                        (b'elapsed_us=190000000',b'elapsed_us=100000000'),
                        (b'elapsed_us=180000000',b'elapsed_us=50000000'),
                        (b'members=0',b'members=1'),(b'token=1 runtime_retired',b'token=2 runtime_retired'),
                        (b'state=4;',b'state=2;'),(b'held=1;',b'held=0;'),(b'heap=0',b'heap=1'),
                        (b'final=1',b'final=0'),(b'events=2100',b'events=1'),(b'screenshot=yes',b'screenshot=NO'),
                        (b'native=explicit-capture',b'native=disabled')):
            with self.subTest(old=old),self.assertRaises(AssertionError):check_capture(data.replace(old,new),vm=True)
        line=b'battery-desktop-lifetime: drained token=1 members=0 peak=4 revoked=yes\n'
        bad=data.replace(line,b'').replace(b'battery-workspace-hold: released',line+b'battery-workspace-hold: released')
        with self.assertRaises(AssertionError):check_capture(bad,vm=True)
        with self.assertRaises(AssertionError):check_capture(data+b'workspace-log: SAVE FAILED\n',vm=True)

if __name__=='__main__':unittest.main()
