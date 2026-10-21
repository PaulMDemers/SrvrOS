import hashlib
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
if __name__=='__main__':unittest.main()
