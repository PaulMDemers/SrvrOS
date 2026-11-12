import unittest
from sidecar_equality import sidecar_equality
class Tests(unittest.TestCase):
    def fixture(self):return b'exfat-sidecar: version=1 snapshot_us=1000 checks=10 equal=4 changed=6 refused=0 retained_bytes=4096 flush_failed=0 active=0 invalid=0\n'
    def test_complete(self):self.assertEqual(sidecar_equality([self.fixture(),self.fixture()])['equal'],4)
    def test_differences(self):
        diff=b'exfat-sidecar-diff: version=1 snapshot_us=1000 shape=1 path=2 inode=0 mode=1 nlink=0 uid=0 gid=0 atime=2 mtime=0 ctime=0 format=0 last_offset=100\n'
        raw=self.fixture()+diff
        self.assertEqual(sidecar_equality([raw],True)['differences']['path'],2)
        for a,b in [(b'shape=1',b'shape=2'),(b'snapshot_us=1000',b'snapshot_us=1001'),(b'path=2',b'path=18446744073709551616'),(b'format=0',b'format=x')]:
            with self.assertRaises(ValueError):sidecar_equality([self.fixture()+diff.replace(a,b)],True)
        with self.assertRaises(ValueError):sidecar_equality([self.fixture()],True)
        with self.assertRaises(ValueError):sidecar_equality([raw+diff.replace(b'last_offset=100',b'last_offset=101')],True)
    def test_changed_only_diagnostic(self):
        raw=self.fixture().replace(b'equal=4',b'equal=0').replace(b'changed=6',b'changed=10').replace(b'retained_bytes=4096',b'retained_bytes=0')
        with self.assertRaises(ValueError):sidecar_equality([raw])
        self.assertEqual(sidecar_equality([raw],require_equal=False)['changed'],10)
    def test_refuse(self):
        for a,b in [(b'version=1',b'version=2'),(b'checks=10',b'checks=11'),(b'equal=4',b'equal=0'),(b'retained_bytes=4096',b'retained_bytes=1'),(b'refused=0',b'refused=1'),(b'flush_failed=0',b'flush_failed=1'),(b'active=0',b'active=1'),(b'invalid=0',b'invalid=1')]:
            with self.assertRaises(ValueError):sidecar_equality([self.fixture().replace(a,b)])
        with self.assertRaises(ValueError):sidecar_equality([])
        with self.assertRaises(ValueError):sidecar_equality([self.fixture(),self.fixture().replace(b'equal=4',b'equal=5')])
if __name__=='__main__':unittest.main()
