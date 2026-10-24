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


def transitions_fixture():
    data=fixture().replace(b'battery-home-desktop-capture: begin seconds=180 watchdog_seconds=240 startup=disabled input=none',
        b'battery-home-transitions-capture: begin phases=3 watchdog_seconds=480 startup=disabled input=AC-unplug-reconnect')
    data=data.replace(b'battery-home-desktop-sample:',b'battery-home-transitions-sample:').replace(b'battery-home-desktop-capture: returned',b'battery-home-transitions-capture: returned')
    phases=[]
    for phase in range(3):
        first=phase*10+1;ac=int(phase!=1);sample=(phase+1)*20000000
        phases.append((f'desktop-ac: accepted={phase} stable=3 epoch=1 first_gen={first} last_gen={first+2} ac={ac} soc=86 current_ua={0 if ac else -250000} voltage_uv=12000000 remaining=4300000 full=5000000 unit=1 ac_us={sample-1000} sampled_us={sample} age_us=500 label={"AC · " if ac else ""}86%\n'
            f'deskd: screenshot captured path=/work/srvros-work/Screenshots/Screen-{phase}.bmp width=2 height=2\n'
            f'deskd: screenshot saved status=0 path=/work/srvros-work/Screenshots/Screen-{phase}.bmp\n'
            f'desktop-ac: screenshot phase={phase} saved=yes\n'+
            (f'desktop-ac: prompt phase={phase+1}\n' if phase<2 else '')).encode())
    begin=data.index(b'desktop-idle: elapsed_us=');end=data.index(b'battery-home-transitions-sample:')
    return data[:begin]+b''.join(phases)+b'desktop-ac: completed=yes phases=3 screenshots=3\n'+data[end:]


def work_fixture():
    data=fixture().replace(b'battery-home-desktop-capture: begin seconds=180 watchdog_seconds=240 startup=disabled input=none',
        b'battery-home-work-capture: begin seconds=300 watchdog_seconds=480 startup=disabled input=typing-files')
    start=data.index(b'desktop-idle: elapsed');end=data.index(b'battery-home-desktop-sample:')
    data=data[:start]+b"""desktop-work: begin minimum_us=300000000 deadline_us=480000000
desktools: file-result role=text op=2 status=0 bytes=20
desktools: file-result role=text op=1 status=0 bytes=20
desktools: file-result role=files op=3 status=0 bytes=100
desktop-work: ready-to-exit screenshot=yes
desktop-work: completed=yes elapsed_us=301000000 keys=40 pointer_events=5 screenshot=yes normal_exit=1
"""+data[end:]
    return data.replace(b'battery-home-desktop-sample:',b'battery-home-work-sample:').replace(b'battery-home-desktop-capture: returned',b'battery-home-work-capture: returned').replace(b'elapsed_us=190000000',b'elapsed_us=302000000')


class Records(unittest.TestCase):
    def test_work_requires_duration_input_io_and_normal_exit(self):
        data=work_fixture();check_capture(data,vm=True,work=True)
        for old,new in ((b'elapsed_us=301000000',b'elapsed_us=299000000'),(b'elapsed_us=302000000',b'elapsed_us=480000000'),
                        (b'keys=40',b'keys=0'),(b'pointer_events=5',b'pointer_events=0'),(b'normal_exit=1',b'normal_exit=0'),
                        (b'op=2 status=0',b'op=2 status=-1'),(b'op=1 status=0',b'op=1 status=-1'),
                        (b'role=files op=3',b'role=files op=8'),(b'last_gen=66',b'last_gen=60')):
            with self.subTest(old=old),self.assertRaises(AssertionError):check_capture(data.replace(old,new),vm=True,work=True)
        with self.assertRaises(AssertionError):check_capture(data,vm=True)
        for row in data.splitlines(keepends=True):
            if row.startswith(b'desktop-work:'):
                for bad in (data.replace(row,b''),data+row):
                    with self.assertRaises(AssertionError):check_capture(bad,vm=True,work=True)

    def test_transitions_require_ordered_distinct_samples_and_saved_images(self):
        data=transitions_fixture();result=check_capture(data,vm=True,transitions=True)
        self.assertEqual([p['ac'] for p in result['phases']],[True,False,True])
        for old,new in ((b'last_gen=3',b'last_gen=2'),(b'first_gen=11',b'first_gen=1'),
                        (b'current_ua=-250000',b'current_ua=0'),(b'ac=0',b'ac=1'),
                        (b'epoch=1 first_gen=11',b'epoch=2 first_gen=11'),
                        (b'age_us=500',b'age_us=5000001'),(b'label=86%','label=AC · 86%'.encode()),
                        (b'ac_us=39999000',b'ac_us=19999000'),(b'full=5000000',b'full=0'),
                        (b'Screen-1.bmp',b'Screen-0.bmp'),(b'completed=yes phases=3',b'completed=yes phases=2')):
            with self.subTest(old=old),self.assertRaises(AssertionError):check_capture(data.replace(old,new),vm=True,transitions=True)
        for row in data.splitlines(keepends=True):
            if row.startswith((b'desktop-ac:',b'deskd: screenshot')):
                for bad in (data.replace(row,b'',1),data+row):
                    with self.subTest(row=row),self.assertRaises(AssertionError):check_capture(bad,vm=True,transitions=True)
        with self.assertRaises(AssertionError):check_capture(data,vm=True)

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
