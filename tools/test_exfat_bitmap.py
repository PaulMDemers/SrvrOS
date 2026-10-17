import io
import struct
import subprocess
import tempfile
from pathlib import Path
import unittest
import repair_exfat_bitmap as repair
from exfat_test_image import ExfatFixture, check_clean_image


class BitmapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'volume.img'
            with path.open('wb') as f:f.truncate(32*1024*1024)
            subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(path)],check=True,stdout=subprocess.DEVNULL)
            cls.original=path.read_bytes()
        fs=ExfatFixture(cls.original)
        cls.volume=f'{fs.u32(100):08x}'
        root=fs.chain(fs.root)
        cls.offset=fs.offset(fs.root)+next(i for i in range(0,len(root),32) if root[i]==0x81)
        cls.length=struct.unpack_from('<Q',cls.original,cls.offset+24)[0]

    def malformed(self,excess=1):
        data=bytearray(self.original)
        struct.pack_into('<Q',data,self.offset+24,self.length+excess)
        return data

    def test_exact_repair_and_independent_checker(self):
        self.assertIsNone(repair.plan(io.BytesIO(self.original),self.volume))
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'bad.img';path.write_bytes(self.malformed())
            check=subprocess.run(['/usr/sbin/fsck.exfat','-n',str(path)],capture_output=True,text=True)
            self.assertIn('invalid bitmap size',check.stdout+check.stderr)
            with self.assertRaises(RuntimeError): check_clean_image(path)
            with path.open('r+b') as f:
                proposed=repair.plan(f,self.volume)
                self.assertEqual(proposed['old_length'],self.length+1)
                repair.apply(f,proposed)
            self.assertEqual(path.read_bytes(),self.original)
            check=subprocess.run(['/usr/sbin/fsck.exfat','-n',str(path)],capture_output=True,text=True)
            self.assertEqual(check.returncode,0)
            self.assertNotIn('ERROR',check.stdout+check.stderr)
            check_clean_image(path)

    def test_refuse_unknown_geometry_and_identity(self):
        for data,volume in ((self.malformed(2),self.volume),(self.malformed(),'00000000')):
            with self.assertRaises(ValueError):repair.plan(io.BytesIO(data),volume)

    def test_refuse_changed_original(self):
        proposed=repair.plan(io.BytesIO(self.malformed()),self.volume)
        with tempfile.TemporaryFile() as f:
            f.write(self.original);f.flush()
            with self.assertRaises(ValueError):repair.apply(f,proposed)
            f.seek(0);self.assertEqual(f.read(),self.original)


if __name__=='__main__':unittest.main()
