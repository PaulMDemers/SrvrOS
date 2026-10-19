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
