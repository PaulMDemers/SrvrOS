import tempfile
import unittest
from pathlib import Path
import hardware_desktop as desktop

class CollectionTests(unittest.TestCase):
    def test_device_summary_order_and_attention(self):
        newer="workspace-devices: audio_state=4 error=3 started=2 completed=1 monotonic_us=200"
        older="workspace-devices: audio_state=2 error=0 started=1 completed=1 monotonic_us=100"
        self.assertEqual(desktop.device_summaries(newer+"\n"+older)["audio"]["monotonic_us"],200)
        self.assertTrue(desktop.needs_attention(newer))
        self.assertFalse(desktop.needs_attention(older))
        self.assertTrue(desktop.needs_attention("smc: state=4 flags=0 error=1"))
        self.assertFalse(desktop.needs_attention("smc: state=1 flags=0 error=0"))
        critical="workspace-devices: thermal_state=2 flags=0x0000000000000008 core_mc=-1000 monotonic_us=300"
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
            (folder/'exit.log').unlink();(folder/'exit.log').symlink_to(folder/'boot.log')
            with self.assertRaises(ValueError): desktop.collect(mount,'37d8c5d0',root/'out')
            (folder/'exit.log').unlink();(folder/'exit.log').write_bytes(b'x'*65537)
            with self.assertRaises(ValueError): desktop.collect(mount,'37d8c5d0',root/'out')
            with self.assertRaises(ValueError): desktop.collect(mount,'12345678',root/'out')

if __name__=='__main__': unittest.main()
