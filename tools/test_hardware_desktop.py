import tempfile
import unittest
from pathlib import Path
import hardware_desktop as desktop

class CollectionTests(unittest.TestCase):
    def test_logs_and_unsafe_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);mount=root/'usb';work=mount/'srvros-work';folder=work/'Logs/session-0000000000000001-00'
            folder.mkdir(parents=True);(work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            (folder/'boot.log').write_text('laptop-probe: unsupported chipset\n')
            (folder/'session-a.log').write_text('deskd: closing pid=3 reason=input-timeout\n')
            (folder/'exit.log').write_text('deskd: exited frames=20\n')
            out,report=desktop.collect(mount,'37d8c5d0',root/'out');item=report['sessions'][0]
            self.assertTrue(item['exit_snapshot_present'] and item['desktop_exit_seen'])
            self.assertEqual(len(item['attention']),1);self.assertEqual(len(item['probe']),1)
            self.assertEqual((out/item['session']/'exit.log').read_bytes(),(folder/'exit.log').read_bytes())
            self.assertEqual(report['hardware_acceptance'],'manual-pending')
            (folder/'exit.log').unlink();(folder/'exit.log').symlink_to(folder/'boot.log')
            with self.assertRaises(ValueError): desktop.collect(mount,'37d8c5d0',root/'out')
            (folder/'exit.log').unlink();(folder/'exit.log').write_bytes(b'x'*65537)
            with self.assertRaises(ValueError): desktop.collect(mount,'37d8c5d0',root/'out')
            with self.assertRaises(ValueError): desktop.collect(mount,'12345678',root/'out')

if __name__=='__main__': unittest.main()
