import hashlib
import io
from pathlib import Path
import tempfile
import tarfile
import unittest
import wifi_wpa5_credentials as c
class PrivatePMK(unittest.TestCase):
 def test_provision(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);source=root/'credentials';out=root/'record';source.write_text('Synthetic5\npassword\n\nOther\ndifferent\n')
   c.provision(source,'Synthetic5',out);blob=c.read_private(out)
   self.assertEqual(blob[41:],hashlib.pbkdf2_hmac('sha1',b'password',b'Synthetic5',4096,32));self.assertEqual(out.stat().st_mode&0o777,0o600)
   with self.assertRaises(FileExistsError):c.provision(source,'Synthetic5',out)
   out.chmod(0o644)
   with self.assertRaises(ValueError):c.read_private(out)
   out.chmod(0o600);(root/'link').symlink_to(out)
   with self.assertRaises(OSError):c.read_private(root/'link')
   for text in ('Synthetic5\nshort\n','Synthetic5\npassword\nSynthetic5\npassword\n','Other\npassword\n','Synthetic5\n','Synthetic5\n'+'z'*64+'\n'):
    source.write_text(text)
    with self.assertRaises(ValueError):c.derive(source,'Synthetic5')
 def test_tar(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);source=root/'credentials';out=root/'record';archive=root/'initramfs.tar';source.write_text('Synthetic5\npassword\n');c.provision(source,'Synthetic5',out)
   with tarfile.open(archive,'w',format=tarfile.USTAR_FORMAT):pass
   c.verify_initramfs(archive,False)
   with self.assertRaises(ValueError):c.verify_initramfs(archive,True)
   c.add_to_initramfs(out,archive);c.verify_initramfs(archive,True)
   self.assertEqual(archive.stat().st_mode&0o777,0o600)
   with self.assertRaises(ValueError):c.add_to_initramfs(out,archive)
   with self.assertRaises(ValueError):c.verify_initramfs(archive,False)
   with tarfile.open(archive,'a',format=tarfile.USTAR_FORMAT) as tar:
    info=tarfile.TarInfo(c.NAME);info.size=73;info.mode=0o600;tar.addfile(info,io.BytesIO(c.read_private(out)))
   with self.assertRaises(ValueError):c.verify_initramfs(archive,True)
if __name__=='__main__':unittest.main()
