"""Read-only workspace diagnostics using the actual native kernel importer."""
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import subprocess
import tempfile
import unittest
from unittest import mock
from test_storage_leases import empty_image
import workspace_inspection as inspect

ROOT=Path(__file__).resolve().parents[1]
DRIVER=ROOT/'build/tests/installation-inspect-driver'

def snapshot(path):
    st=path.stat();return (hashlib.sha256(path.read_bytes()).hexdigest(),inspect.stamp(st))

class WorkspaceInspectionTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve();self.image=self.root/'volume.img'
        self.fs=empty_image(8*1024*1024,serial=0x1234abcd)
        self.fs.add_directory_files('srvros-work',{'.workspace':b'SRVROS-WORKSPACE-V1 1234abcd\n','source':b'committed'})
        self.image.write_bytes(self.fs.data)
    def run_inspect(self,expected='inspected'):
        before=snapshot(self.image)
        r=inspect.inspect_workspace(self.image,DRIVER,0,self.image.stat().st_size)
        self.assertEqual(r['state'],expected,r);self.assertEqual(snapshot(self.image),before)
        self.assertEqual(r.get('admission'),'not-authorized');return r
    def test_clean_metadata_only(self):
        r=self.run_inspect();d=r['diagnostic'];self.assertFalse(d['filesystem']['dirty']);self.assertTrue(d['filesystem']['allocation_consistent'])
        self.assertFalse(d['writable_admission']);self.assertFalse(d['sidecar_loaded'])
        self.assertEqual(next(x['size'] for x in d['entries'] if x['path'].endswith('/source')),9)
        self.assertFalse(any('data_hex' in x for x in d['entries']))
    def test_readonly_permissions_retained(self):
        self.image.chmod(0o444);r=self.run_inspect();self.assertEqual(r['image_mode_octal'],'0444')
    def test_dirty_state_is_not_clean_or_admission(self):
        struct.pack_into('<H',self.fs.data,106,2);self.image.write_bytes(self.fs.data)
        r=self.run_inspect();self.assertTrue(r['diagnostic']['filesystem']['dirty']);self.assertTrue(r['diagnostic']['filesystem']['allocation_consistent'])
    def test_clean_flags_allocation_leak_is_reported(self):
        root=self.fs.chain(self.fs.root);bitmap=struct.unpack_from('<I',root,20)[0]
        for bit in range(self.fs.count-1,-1,-1):
            offset=self.fs.offset(bitmap)+bit//8
            if not self.fs.data[offset]&(1<<(bit%8)):
                self.fs.data[offset]|=1<<(bit%8);break
        self.image.write_bytes(self.fs.data);r=self.run_inspect();a=r['diagnostic']['filesystem']['allocation']
        self.assertFalse(r['diagnostic']['filesystem']['allocation_consistent']);self.assertEqual(a['leaked'],1)
    def test_pending_sidecar_is_not_replayed(self):
        self.fs.add_directory_files('.srvros',{'meta.tmp':b'SRVROSMETA1\n','meta':b'not-authoritative'})
        self.image.write_bytes(self.fs.data);r=self.run_inspect();self.assertFalse(r['diagnostic']['sidecar_loaded'])
        fd=os.open(self.image,os.O_RDONLY)
        try:
            strict=subprocess.run([str(DRIVER),'--fd',str(fd),'0',str(len(self.fs.data))],pass_fds=(fd,),capture_output=True)
        finally:os.close(fd)
        self.assertNotEqual(strict.returncode,0);self.assertEqual(strict.stdout,b'')
    def test_missing_marker_refused(self):
        self.fs=empty_image(8*1024*1024);self.image.write_bytes(self.fs.data);self.run_inspect('refused')
    def test_bad_marker_refused(self):
        self.fs.data=self.fs.data.replace(b'SRVROS-WORKSPACE-V1 1234abcd\n',b'SRVROS-WORKSPACE-V1 1234abcZ\n');self.image.write_bytes(self.fs.data);self.run_inspect('refused')
    def test_bad_boot_refused(self):
        self.image.write_bytes(bytes(len(self.fs.data)));self.run_inspect('refused')
    def test_ranges_refused(self):
        before=snapshot(self.image)
        for offset,length in ((-1,512),(1,512),(0,0),(0,513),(0,self.image.stat().st_size+512),(2**80,512)):
            r=inspect.inspect_workspace(self.image,DRIVER,offset,length);self.assertEqual(r['state'],'refused')
        self.assertEqual(snapshot(self.image),before)
    def test_symlink_and_hardlink_refused(self):
        alias=self.root/'alias';alias.symlink_to(self.image);self.assertEqual(inspect.inspect_workspace(alias,DRIVER,0,len(self.fs.data))['state'],'refused')
        alias.unlink();alias.hardlink_to(self.image);self.run_inspect('refused')
    def test_native_writable_fd_refused(self):
        before=snapshot(self.image);fd=os.open(self.image,os.O_RDWR)
        try:r=subprocess.run([str(DRIVER),'--workspace-fd',str(fd),'0',str(len(self.fs.data))],pass_fds=(fd,),capture_output=True)
        finally:os.close(fd)
        self.assertNotEqual(r.returncode,0);self.assertEqual(r.stdout,b'');self.assertEqual(snapshot(self.image),before)
    def test_driver_failure_still_verifies_image(self):
        with mock.patch.object(inspect.native,'driver_output',side_effect=ValueError('refused')):
            r=self.run_inspect('refused');self.assertTrue(r['image_unchanged'])
    def test_mutation_detection_does_not_claim_unchanged(self):
        def change(*args):self.image.chmod(0o400);raise ValueError('injected permission change')
        with mock.patch.object(inspect.native,'driver_output',side_effect=change):
            r=inspect.inspect_workspace(self.image,DRIVER,0,len(self.fs.data))
        self.assertEqual(r['state'],'refused');self.assertFalse(r['image_unchanged'])

if __name__=='__main__':unittest.main()
