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

    def test_battery_requires_five_fresh_samples_and_clean_retirement(self):
        rows=[f"battery: sample={i} ac_present=1 soc=86 voltage_uv=12000000 current_ua=-250000 capacity=4300000 full=5000000 unit=1 temp_mc=24950 valid=0x3f age_us=100 ac_us={i*1000} start_us={i*1000+1} end_us={i*1000+500}" for i in range(1,6)]
        tail=["battery: PASS samples=5 notifications=2 queries=1 attached=1 detached=1 heap=0 errors=0 denied=0; physical accuracy NOT_TESTED",
              "battery-capture: returned=yes clean=yes; physical accuracy NOT_TESTED"]
        text='\n'.join(rows+tail)
        result=desktop.battery_diagnostic(text)
        self.assertTrue(result['runtime_complete']);self.assertEqual(result['physical_accuracy'],'manual-pending')
        self.assertEqual(result['samples'][0]['capacity_unit'],'uAh')
        for bad in ('\n'.join(rows+tail[:1]),text+'\n'+tail[-1],text.replace('sample=5','sample=4'),
                    text.replace('age_us=100','age_us=1000001'),text.replace('soc=86','soc=101'),
                    text.replace('valid=0x3f','valid=0x01'),text.replace('heap=0','heap=1'),
                    text.replace('detached=1','detached=0'),text.replace('clean=yes','clean=NO'),
                    text+'\nbattery: HALTED test',text.replace('end_us=1500','end_us=999')):
            self.assertIsNone(desktop.battery_diagnostic(bad),bad)
        held='battery: foreign-event held enable=0x189c original=0x40'
        restored='battery: foreign-event restored enable=0x189c original=0x40 protected_writes=1'
        with_event='\n'.join([held]+rows+[restored]+tail)
        self.assertTrue(desktop.battery_diagnostic(with_event)['foreign_event']['restored'])
        for bad in (with_event.replace(restored,''),with_event.replace(held,''),
                    with_event.replace(restored,restored+'\n'+restored),
                    with_event.replace(restored,restored.replace('0x40','0x80')),
                    with_event.replace('protected_writes=1','protected_writes=0'),
                    with_event.replace(restored,'')+'\n'+restored):
            self.assertIsNone(desktop.battery_diagnostic(bad),bad)
        held2=held.replace('0x189c','0x189e').replace('0x40','0x04')
        restored2=restored.replace('0x189c','0x189e').replace('0x40','0x04')
        multiple='\n'.join([held,held2]+rows+[restored,restored2]+tail)
        self.assertEqual(len(desktop.battery_diagnostic(multiple)['foreign_events']),2)
        for bad in (multiple.replace(restored2,''),multiple.replace(held2,held),
                    multiple.replace(restored2,restored),multiple.replace(restored2,restored2.replace('0x04','0x40'))):
            self.assertIsNone(desktop.battery_diagnostic(bad),bad)
        self.assertTrue(desktop.needs_attention('battery: HALTED operation deadline'))
        self.assertTrue(desktop.needs_attention('battery: REFUSED platform/resource ownership preflight'))

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

    def test_boot_only_session_is_incomplete(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);mount=root/'usb';work=mount/'srvros-work'
            folder=work/'Logs/session-0000000000000001-00';folder.mkdir(parents=True)
            (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            (folder/'boot.log').write_text('laptop-capture: boot options=srvros.desktop-release=6a2f90f72a0e4b96 srvros.capture=battery\n')
            _,report=desktop.collect(mount,'37d8c5d0',root/'out');item=report['sessions'][0]
            self.assertFalse(item['exit_snapshot_present'])
            self.assertIsNone(item['battery_diagnostic'])
            self.assertIn('collection: missing exit snapshot; session incomplete',item['attention'])
            self.assertEqual(report['hardware_acceptance'],'manual-pending')

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
