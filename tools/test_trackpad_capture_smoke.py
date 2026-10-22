import binascii
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import trackpad_capture_smoke as smoke

class CaptureHarnessTests(unittest.TestCase):
    def test_stage_identity_and_profile_refusal(self):
        release='0123456789abcdef'
        manifest={'config_version':'ram-usb-v10-trackpad-1234abcd-fedcba9876543210'}
        with patch.object(smoke.staged_boot,'verify',return_value=manifest) as verify:
            source,got,volume,run=smoke.staged_capture(release)
            self.assertEqual((got,volume,run),(manifest,'1234abcd','fedcba9876543210'))
            verify.assert_called_once_with(source)
            for version in (11,12):
                manifest['config_version']=f'ram-usb-v{version}-trackpad-1234abcd-fedcba9876543210'
                self.assertEqual(smoke.staged_capture(release)[2:],('1234abcd','fedcba9876543210'))
            for version in ('ram-usb-v13-trackpad-1234abcd-fedcba9876543210','ram-usb-v7-workspace-1234abcd'):
                manifest['config_version']=version
                with self.assertRaises(ValueError):smoke.staged_capture(release)
        with patch.object(smoke.staged_boot,'verify') as verify:
            with self.assertRaises(ValueError):smoke.staged_capture('../latest')
            verify.assert_not_called()
        with patch.object(smoke.staged_boot,'verify',side_effect=ValueError('hash mismatch')):
            with self.assertRaises(ValueError):smoke.staged_capture(release)

    def test_workspace_layout_and_checksums(self):
        payload=bytes(range(256))*4
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'disk'
            for table in ('mbr','gpt'):
                smoke.workspace_disk(path,payload,table);data=path.read_bytes()
                self.assertEqual(data[2048*512:2048*512+len(payload)],payload)
                self.assertEqual(data[510:512],b'\x55\xaa')
                if table=='mbr':
                    self.assertEqual(data[450],7)
                    self.assertEqual(struct.unpack_from('<II',data,454),(2048,2))
                else:
                    self.assertEqual(data[450],0xee)
                    self.assertEqual(struct.unpack_from('<II',data,454),(1,0xffffffff))
                    for offset in (512,len(data)-512):
                        header=bytearray(data[offset:offset+92]);self.assertEqual(header[:8],b'EFI PART')
                        crc=struct.unpack_from('<I',header,16)[0];struct.pack_into('<I',header,16,0)
                        self.assertEqual(binascii.crc32(header)&0xffffffff,crc)
                    entries=data[1024:1024+128*128]
                    self.assertEqual(binascii.crc32(entries)&0xffffffff,struct.unpack_from('<I',data,512+88)[0])
                    self.assertEqual(struct.unpack_from('<QQ',entries,32),(2048,2049))
                    self.assertEqual(entries,data[-512-128*128:-512])

if __name__=='__main__':unittest.main()
