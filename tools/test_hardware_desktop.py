import tempfile
import unittest
from pathlib import Path
import hardware_desktop as desktop

class CollectionTests(unittest.TestCase):
    def test_release_identity_requires_complete_boot_option(self):
        release="0620ce5e026b2f32"
        text=(f"cmdline: srvros.desktop-release={release}\n"
              f"desktop-trial: boot options=srvros.desktop-release={release}\n"
              "terminal: srvros.desktop-release=ffffffffffffffff\n"
              "cmdline: srvros.desktop-release=11111111111111111\n"
              "cmdline: srvros.desktop-release=2222222222222222suffix\n")
        self.assertEqual(desktop.session_releases(text),[release])
        self.assertEqual(desktop.session_releases("desktop started\n"),[])

    def test_idle_measurement_requires_complete_exit_record(self):
        record="idle-capture: elapsed_us=60000000 sampled_ticks=6000 idle_ticks=5700 switches=12000 preemptions=25 completed=yes"
        self.assertEqual(desktop.idle_sample(record)["idle_sample_percent"],95)
        for bad in (record.replace("60000000","1000"),record.replace("sampled_ticks=6000","sampled_ticks=0"),
                    record.replace("idle_ticks=5700","idle_ticks=6001"),record.replace("completed=yes","completed=NO"),
                    "terminal: "+record, record+"\n"+record, ""):
            self.assertIsNone(desktop.idle_sample(bad))
        self.assertEqual(desktop.session_releases("laptop-capture: boot options=srvros.desktop-release=0123456789abcdef"),["0123456789abcdef"])

    def test_desktop_idle_requires_saved_capture_and_successful_return(self):
        line="desktop-idle: elapsed_us=60000000 sampled_ticks=6000 idle_ticks=5400 switches=12000 preemptions=25 frames=120 fb_bytes=9000 measured=yes"
        tail="\ndesktop-idle: completed=yes screenshot=yes\ndesktop-idle-capture: returned=yes clean=yes"
        record=line+tail
        self.assertEqual(desktop.desktop_idle_sample(record)["idle_sample_percent"],90)
        for bad in (line,record.replace("clean=yes","clean=NO"),record.replace("screenshot=yes","screenshot=NO"),
                    record.replace("60000000","1000"),record.replace("idle_ticks=5400","idle_ticks=6001"),
                    record+"\n"+line,record+"\ndeskd: FAIL", "terminal: "+record):
            self.assertIsNone(desktop.desktop_idle_sample(bad))

    def test_device_summary_order_and_attention(self):
        newer="workspace-devices: audio_state=4 error=3 started=2 completed=1 monotonic_us=200"
        older="workspace-devices: audio_state=2 error=0 started=1 completed=1 monotonic_us=100"
        self.assertEqual(desktop.device_summaries(newer+"\n"+older)["audio"]["monotonic_us"],200)
        self.assertTrue(desktop.needs_attention(newer))
        self.assertFalse(desktop.needs_attention(older))
        self.assertTrue(desktop.needs_attention("smc: state=4 flags=0 error=1"))
        self.assertFalse(desktop.needs_attention("smc: state=1 flags=0 error=0"))
        critical="workspace-devices: thermal_state=2 flags=0x0000000000000008 core_mc=-1000 monotonic_us=300"
        self.assertTrue(desktop.needs_attention("workspace-devices: pcm_state=6 underruns=0 monotonic_us=500"))
        self.assertTrue(desktop.needs_attention("workspace-devices: pcm_state=4 underruns=1 monotonic_us=500"))
        self.assertFalse(desktop.needs_attention("workspace-devices: pcm_state=4 underruns=0 monotonic_us=500"))
        self.assertTrue(desktop.needs_attention(critical))
        self.assertEqual(desktop.device_snapshot(critical)[1]["core_mc"],-1000)
        for bad in ("workspace-devices: audio_state=4 monotonic_us=oops","workspace-devices: audio_state=4","audio_state=4 monotonic_us=1"):
            self.assertIsNone(desktop.device_snapshot(bad))

    def test_power_samples_remain_diagnostic(self):
        good="workspace-devices: power_state=2 flags=3 package_mw=6010 active_mhz=1424 rejected=0 monotonic_us=200"
        stale=good.replace("flags=3","flags=4").replace("monotonic_us=200","monotonic_us=300")
        self.assertFalse(desktop.needs_attention(good))
        self.assertTrue(desktop.needs_attention(stale))
        self.assertTrue(desktop.needs_attention(good.replace("rejected=0","rejected=1")))
        self.assertEqual(desktop.device_summaries(stale+"\n"+good)["power"]["flags"],4)

    def test_power_uses_only_covered_idle_interval(self):
        header=("desktop-idle: elapsed_us=60000000 sampled_ticks=6000 idle_ticks=5400 switches=12000 preemptions=25 frames=1 fb_bytes=9000 measured=yes\n"
                "desktop-idle-window: start_us=10000000 end_us=70000000\n"
                "desktop-idle: completed=yes screenshot=yes\ndesktop-idle-capture: returned=yes clean=yes\n")
        def sample(at,energy,a,m,n):
            return (f"workspace-devices: power_state=2 flags=3 energy_shift=14 nominal_mhz=2200 sample_us={at} interval_us=1000000 "
                    f"samples={n} rejected=0 energy={energy} aperf={a} mperf={m} monotonic_us={at+100000}\n")
        one=sample(20000000,0xffffff00,100,100,20)
        two=sample(30000000,(0xffffff00+1638400)&0xffffffff,15000000100,10000000100,30)
        three=sample(40000000,(0xffffff00+3276800)&0xffffffff,30000000100,20000000100,40)
        text=header+one+two+three
        result=desktop.power_idle_sample(text)
        self.assertEqual((result['covered_us'],result['desktop_window_us'],result['average_package_mw'],result['active_cpu0_mhz']),(20000000,60000000,10000,3300))
        # Screenshot save is outside the measurement window; never include it.
        later=sample(80000000,0,0,0,80)
        self.assertEqual(desktop.power_idle_sample(text+later),result)
        self.assertEqual(desktop.power_idle_sample(text+two.replace('monotonic_us=30100000','monotonic_us=30200000')),result)
        for bad in (text.replace('clean=yes','clean=NO'),text.replace('end_us=70000000','end_us=69999999'),
                    text.replace('energy_shift=14','energy_shift=16'),text.replace('rejected=0','rejected=1'),
                    text.replace('power_state=2','power_state=1'),text.replace('flags=3','flags=4'),
                    text.replace('nominal_mhz=2200','nominal_mhz=0'),text.replace('sample_us=30000000','sample_us=26000000'),
                    header+one+three,header+one+two+three.replace('samples=40','samples=30'),
                    text+two.replace('energy=1638144','energy=3'),text.replace('mperf=20000000100','mperf=1'),
                    header+one+two+three.replace('energy=3276544','energy=0')):
            self.assertIsNone(desktop.power_idle_sample(bad),bad)

    def test_logs_and_unsafe_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);mount=root/'usb';work=mount/'srvros-work';folder=work/'Logs/session-0000000000000001-00'
            folder.mkdir(parents=True);(work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            (folder/'boot.log').write_text('laptop-probe: unsupported chipset\n')
            (folder/'session-a.log').write_text('deskd: closing pid=3 reason=input-timeout\n')
            (folder/'exit.log').write_text('backlight: provider=ready initial_percent=14\ndeskd: brightness requested=50 result=-1\ndeskd: exited frames=20\n')
            out,report=desktop.collect(mount,'37d8c5d0',root/'out');item=report['sessions'][0]
            self.assertTrue(item['exit_snapshot_present'] and item['desktop_exit_seen'])
            self.assertEqual(len(item['brightness']),2);self.assertEqual(len(item['attention']),2);self.assertEqual(len(item['probe']),1)
            self.assertEqual((out/item['session']/'exit.log').read_bytes(),(folder/'exit.log').read_bytes())
            self.assertEqual(report['hardware_acceptance'],'manual-pending')
            self.assertEqual(item['releases'],[])
            (folder/'boot.log').write_text('cmdline: srvros.desktop-release=0620ce5e026b2f32\n')
            (folder/'exit.log').write_text('cmdline: srvros.desktop-release=d61e60a82a295d84\n')
            _,mixed=desktop.collect(mount,'37d8c5d0',root/'out')
            self.assertEqual(mixed['sessions'][0]['releases'],['0620ce5e026b2f32','d61e60a82a295d84'])
            self.assertIn('collection: conflicting boot release identities',mixed['sessions'][0]['attention'])
            (folder/'exit.log').unlink();(folder/'exit.log').symlink_to(folder/'boot.log')
            with self.assertRaises(ValueError): desktop.collect(mount,'37d8c5d0',root/'out')
            (folder/'exit.log').unlink();(folder/'exit.log').write_bytes(b'x'*65537)
            with self.assertRaises(ValueError): desktop.collect(mount,'37d8c5d0',root/'out')
            with self.assertRaises(ValueError): desktop.collect(mount,'12345678',root/'out')

if __name__=='__main__': unittest.main()
