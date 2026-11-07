import hashlib
import tarfile
import io
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import wifi_firmware_package as pkg

class PackageTests(unittest.TestCase):
    def test_validation_and_atomic_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); image=root/'mt7610e.bin'; license=root/'license'; out=root/'output'
            image.write_bytes(b'firmware');license.write_bytes(b'license')
            with self.assertRaises(ValueError):pkg.package(image,license,out)
            self.assertFalse(out.exists())
            with patch.object(pkg,'inspect',return_value={'sha256':'synthetic'}):
                with self.assertRaises(ValueError):pkg.package(image,license,out)
                self.assertFalse(out.exists())
                with patch.object(pkg,'LICENSE_SHA256',hashlib.sha256(b'license').hexdigest()):
                    result=pkg.package(image,license,out)
                    self.assertFalse(result['installed'])
                    self.assertEqual((out/image.name).read_bytes(),image.read_bytes())
                    self.assertEqual((out/'LICENCE.mediatek').read_bytes(),license.read_bytes())
                    with self.assertRaises(ValueError):pkg.package(image,license,out)
                    self.assertEqual((out/image.name).read_bytes(),b'firmware')
    def test_staged_tar_contents_and_duplicates(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'mt7610e.bin').write_bytes(b'firmware');(root/'LICENCE.mediatek').write_bytes(b'license')
            archive=root/'initramfs.tar'
            with tarfile.open(archive,'w',format=tarfile.USTAR_FORMAT) as tar:
                info=tarfile.TarInfo('init');info.size=4;tar.addfile(info,io.BytesIO(b'init'))
            with patch.object(pkg,'inspect',return_value={}),patch.object(pkg,'LICENSE_SHA256',hashlib.sha256(b'license').hexdigest()):
                pkg.add_to_initramfs(root,archive);pkg.verify_initramfs(archive)
                with tarfile.open(archive) as tar:self.assertEqual(tar.extractfile('init').read(),b'init')
                before=archive.read_bytes()
                with self.assertRaises(ValueError):pkg.add_to_initramfs(root,archive)
                self.assertEqual(archive.read_bytes(),before)
                with tarfile.open(archive,'a') as tar:
                    info=tarfile.TarInfo('firmware/mt7610e.bin');info.size=1;tar.addfile(info,io.BytesIO(b'x'))
                with self.assertRaises(ValueError):pkg.verify_initramfs(archive)

if __name__=='__main__':unittest.main()
