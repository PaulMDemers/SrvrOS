import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import hardware_log_archive as logs

class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.mount=self.root/'usb';self.mount.mkdir()
        self.folder=self.mount/'srvros-work/Logs';self.folder.mkdir(parents=True)
        self.dest=self.root/'archive'
    def session(self,n,complete=True):
        p=self.folder/f'session-{n:016x}-00';p.mkdir()
        (p/'boot.log').write_text('boot\n')
        if complete:(p/'exit.log').write_text('workspace-log: snapshot=exit.log\n')
        return p
    def old(self):
        for n in range(10):self.session(n)
        return logs.plan(self.mount,keep=8)
    def test_file_and_directory_reserves_are_separate(self):
        with patch.object(logs,'limits',return_value=(5,10)):
            self.assertTrue(logs.headroom(self.mount,files=3,dirs=0)['ok'])
            self.assertFalse(logs.headroom(self.mount,files=4,dirs=0)['ok'])
        with patch.object(logs,'limits',return_value=(100,4)):
            self.assertFalse(logs.headroom(self.mount,files=0,dirs=1)['ok'])
    def test_actual_1023_file_boundary(self):
        meta=self.mount/'.srvros';meta.mkdir();(meta/'meta').write_bytes(b'')
        for n in range(1022):(self.mount/str(n)).write_bytes(b'')
        result=logs.headroom(self.mount,files=1,dirs=0)
        self.assertEqual(result['available_files'],0);self.assertFalse(result['ok'])
        (self.mount/'0').unlink();self.assertTrue(logs.headroom(self.mount,files=1,dirs=0)['ok'])
    def test_preserves_incomplete_unknown_recent(self):
        self.session(0,False);p=self.session(1);(p/'document.txt').write_text('preserve')
        for n in range(2,12):self.session(n)
        self.assertEqual([i['session'] for i in logs.plan(self.mount,8)], [f'session-{n:016x}-00' for n in (2,3)])
        self.assertEqual(len(logs.plan(self.mount,8,1)),1)
    def test_symlinks_refused(self):
        (self.mount/'alias').symlink_to(self.root,target_is_directory=True)
        with self.assertRaises(ValueError):logs.inventory(self.mount)
    def test_same_device_archive_refused(self):
        chosen=self.old()
        with self.assertRaises(ValueError):logs.archive(self.mount,self.dest,chosen,True)
        self.assertEqual(len(list(self.folder.iterdir())),10)
    @patch.object(logs,'archive_location',return_value=True)
    def test_archive_without_prune(self,_):
        chosen=self.old();r=logs.archive(self.mount,self.dest,chosen)
        self.assertEqual(r['state'],'verified');self.assertEqual(len(list(self.folder.iterdir())),10)
        self.assertEqual(logs.snapshot(self.dest/chosen[0]['session']),chosen[0]['files'])
    @patch.object(logs,'archive_location',return_value=True)
    def test_prune_only_verified_batch(self,_):
        chosen=self.old();doc=self.mount/'important.txt';doc.write_text('keep')
        r=logs.archive(self.mount,self.dest,chosen,True)
        self.assertEqual(r['state'],'pruned');self.assertEqual(len(list(self.folder.iterdir())),8)
        self.assertEqual(doc.read_text(),'keep')
        self.assertEqual(json.loads((self.dest/'manifest.json').read_text())['pruned'],[x['session'] for x in chosen])
    @patch.object(logs,'archive_location',return_value=True)
    def test_corrupt_copy_never_prunes(self,_):
        chosen=self.old();copy=logs.shutil.copytree
        def corrupt(src,dst):
            copy(src,dst);(dst/'exit.log').write_text('corrupt')
        with patch.object(logs.shutil,'copytree',side_effect=corrupt):
            with self.assertRaises(ValueError):logs.archive(self.mount,self.dest,chosen,True)
        self.assertEqual(len(list(self.folder.iterdir())),10)
    @patch.object(logs,'archive_location',return_value=True)
    def test_changed_source_never_prunes(self,_):
        chosen=self.old();copy=logs.shutil.copytree
        def changed(src,dst):
            copy(src,dst);(src/'exit.log').write_text('changed')
        with patch.object(logs.shutil,'copytree',side_effect=changed):
            with self.assertRaises(ValueError):logs.archive(self.mount,self.dest,chosen,True)
        self.assertEqual(len(list(self.folder.iterdir())),10)

if __name__=='__main__':unittest.main()
