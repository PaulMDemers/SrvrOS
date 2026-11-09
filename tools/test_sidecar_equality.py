import unittest
from sidecar_equality import sidecar_equality
class Tests(unittest.TestCase):
    def fixture(self):return b'exfat-sidecar: version=1 snapshot_us=1000 checks=10 equal=4 changed=6 refused=0 retained_bytes=4096 flush_failed=0 active=0 invalid=0\n'
    def test_complete(self):self.assertEqual(sidecar_equality([self.fixture(),self.fixture()])['equal'],4)
    def test_refuse(self):
        for a,b in [(b'version=1',b'version=2'),(b'checks=10',b'checks=11'),(b'equal=4',b'equal=0'),(b'retained_bytes=4096',b'retained_bytes=1'),(b'refused=0',b'refused=1'),(b'flush_failed=0',b'flush_failed=1'),(b'active=0',b'active=1'),(b'invalid=0',b'invalid=1')]:
            with self.assertRaises(ValueError):sidecar_equality([self.fixture().replace(a,b)])
        with self.assertRaises(ValueError):sidecar_equality([])
        with self.assertRaises(ValueError):sidecar_equality([self.fixture(),self.fixture().replace(b'equal=4',b'equal=5')])
if __name__=='__main__':unittest.main()
