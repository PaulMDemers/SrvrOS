import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from browser_storage import ResourceError, StorageGuard


class StorageGuardTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name).resolve()
        self.host, self.output, self.backing = (root / name for name in ('host','output','backing'))
        for path in (self.host,self.output,self.backing): path.mkdir()
        self.reserve = 2 * 1024**3
        self.free = {p: self.reserve + 100 for p in (self.host,self.output,self.backing)}
        self.envpatch = patch.dict(os.environ, {}, clear=True)
        self.envpatch.start();self.addCleanup(self.envpatch.stop)
        self.usagepatch = patch('browser_storage.shutil.disk_usage',side_effect=self.disk_usage)
        self.usagepatch.start();self.addCleanup(self.usagepatch.stop)

    def disk_usage(self, path):
        class Usage:
            free = self.free[Path(path)]
        return Usage()

    def guard(self):
        return StorageGuard(self.output, self.host, self.backing, self.reserve)

    def test_each_role_and_no_temp_on_failure(self):
        for role,path in (('host',self.host),('output',self.output),('backing',self.backing)):
            with self.subTest(role=role):
                self.free[path]=self.reserve
                with self.assertRaisesRegex(ResourceError,role): self.guard().environment({'PATH':'keep'})
                self.assertFalse((self.output/'tmp').exists())
                self.free[path]=self.reserve+100

    def test_high_output_does_not_hide_low_backing(self):
        self.free[self.output]=self.reserve+10000;self.free[self.backing]=self.reserve-1
        with self.assertRaisesRegex(ResourceError,'backing'): self.guard().preflight()

    def test_additional_allocation_and_boundary(self):
        g=self.guard()
        with self.assertRaisesRegex(ResourceError,'output'):g.preflight(100)
        g.preflight(99)
        self.free[self.backing]=self.reserve+50
        with self.assertRaisesRegex(ResourceError,'backing'):g.preflight(50)
        self.assertEqual(g.minimum['backing']['free_bytes'],self.reserve+50)
        self.assertEqual(g.snapshot['backing']['path'],str(self.backing))

    def test_environment_preserves_other_values(self):
        g=self.guard();env=g.environment({'PATH':'p','CFLAGS':'-x','HOME':'h','TMPDIR':'old'})
        self.assertEqual(env,{'PATH':'p','CFLAGS':'-x','HOME':'h','TMPDIR':str(self.output/'tmp')})
        self.assertTrue((self.output/'tmp').is_dir())

    def test_temp_symlink_is_refused(self):
        (self.output/'tmp').symlink_to(self.backing,target_is_directory=True)
        with self.assertRaisesRegex(ResourceError,'output temporary'):self.guard().environment({})

    def test_backing_environment_and_bad_paths(self):
        with patch.dict(os.environ,{'SRVROS_BUILD_BACKING_PATH':str(self.backing)}):
            self.assertEqual(StorageGuard(self.output,self.host).paths['backing'],self.backing)
        with self.assertRaisesRegex(ResourceError,'output'):StorageGuard('relative',self.host)
        with self.assertRaisesRegex(ResourceError,'backing'):StorageGuard(self.output,self.host,self.backing/'absent')


if __name__ == '__main__': unittest.main()
