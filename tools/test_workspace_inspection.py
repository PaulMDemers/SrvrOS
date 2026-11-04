"""Read-only workspace diagnostics using the actual native kernel importer."""
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import subprocess
import sys
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
    def test_unexpected_payload_refused(self):
        report=self.run_inspect()['diagnostic'];report['payload']='not-permitted'
        with mock.patch.object(inspect.native,'driver_output',return_value=report):
            self.run_inspect('refused')
    def test_mutation_detection_does_not_claim_unchanged(self):
        def change(*args):self.image.chmod(0o400);raise ValueError('injected permission change')
        with mock.patch.object(inspect.native,'driver_output',side_effect=change):
            r=inspect.inspect_workspace(self.image,DRIVER,0,len(self.fs.data))
        self.assertEqual(r['state'],'refused');self.assertFalse(r['image_unchanged'])


# Small disposable whole-disk fixtures; production/retained media are never edited.
import uuid
import zlib
GPT_DRIVER=ROOT/'build/tests/workspace-gpt-driver'
BASIC=uuid.UUID('ebd0a0a2-b9e5-4433-87c0-68b6b72699c7').bytes_le

def gpt_checksums(data):
    total=len(data)//512
    table=bytes(data[1024:1024+16384]);data[(total-33)*512:(total-1)*512]=table
    for lba in (1,total-1):
        at=lba*512;struct.pack_into('<I',data,at+88,zlib.crc32(table))
        struct.pack_into('<I',data,at+16,0)
        struct.pack_into('<I',data,at+16,zlib.crc32(data[at:at+92]))

def whole_disk(volume,size_bytes=12*1024*1024):
    data=bytearray(size_bytes);total=len(data)//512
    data[510:512]=b'\x55\xaa';data[450]=0xee
    struct.pack_into('<II',data,454,1,total-1)
    disk=uuid.UUID('11111111-2222-3333-4444-555555555555').bytes_le
    for lba,other,table in ((1,total-1,2),(total-1,1,total-33)):
        at=lba*512;data[at:at+8]=b'EFI PART'
        struct.pack_into('<IIIIQQQQ16sQIII',data,at+8,0x10000,92,0,0,lba,other,34,total-34,disk,table,128,128,0)
    data[1024:1040]=BASIC;data[1040:1056]=uuid.UUID('66666666-7777-8888-9999-aaaaaaaaaaaa').bytes_le
    struct.pack_into('<QQ',data,1056,2048,2048+len(volume)//512-1)
    data[2048*512:2048*512+len(volume)]=volume;gpt_checksums(data);return data

class GPTWorkspaceInspectionTests(unittest.TestCase):
    def setUp(self):
        WorkspaceInspectionTests.setUp(self)
        self.disk=whole_disk(self.fs.data);self.image.write_bytes(self.disk)
    def automatic(self,expected='inspected'):
        before=snapshot(self.image)
        r=inspect.inspect_workspace(self.image,DRIVER,gpt_driver=GPT_DRIVER)
        self.assertEqual(r['state'],expected,r);self.assertEqual(snapshot(self.image),before)
        self.assertTrue(r['image_unchanged']);self.assertEqual(r['admission'],'not-authorized');return r
    def test_matches_explicit_range(self):
        r=self.automatic();explicit=inspect.inspect_workspace(self.image,DRIVER,2048*512,len(self.fs.data))
        self.assertEqual(r['diagnostic'],explicit['diagnostic']);self.assertTrue(r['range']['partition_table_validated'])
        self.assertTrue(r['gpt_selection']['backup_agrees']);self.assertEqual(r['range']['offset'],2048*512)
    def test_dirty_readonly_disk(self):
        struct.pack_into('<H',self.disk,2048*512+106,2);self.image.write_bytes(self.disk);self.image.chmod(0o444)
        r=self.automatic();self.assertEqual(r['image_mode_octal'],'0444');self.assertTrue(r['diagnostic']['filesystem']['dirty'])
    def test_corrupt_maps_refused(self):
        total=len(self.disk)//512
        cases={'primary-header':512+16,'primary-table':1024+20,'backup-header':(total-1)*512+16,
               'backup-table':(total-33)*512+20,'protective-mbr':510}
        for name,at in cases.items():
            with self.subTest(name=name):
                bad=self.disk.copy();bad[at]^=1;self.image.write_bytes(bad);self.automatic('refused')
    def test_structurally_invalid_maps_refused(self):
        total=len(self.disk)//512
        def duplicate(d):d[1152:1280]=d[1024:1152];struct.pack_into('<QQ',d,1184,20000,21023)
        def overlap(d):duplicate(d);d[1168]^=1;struct.pack_into('<QQ',d,1184,2048,4095)
        cases={'duplicate-guid':duplicate,'overlap':overlap,
               'range':lambda d:struct.pack_into('<Q',d,1064,total),
               'zero-disk-guid':lambda d:d.__setitem__(slice(568,584),bytes(16)),
               'readonly-attribute':lambda d:struct.pack_into('<Q',d,1072,1<<60),
               'oversized-table':lambda d:struct.pack_into('<I',d,592,2**30),
               'primary-current-lba':lambda d:struct.pack_into('<Q',d,536,2),
               'primary-alternate-lba':lambda d:struct.pack_into('<Q',d,544,total-2),
               'usable-array-overlap':lambda d:struct.pack_into('<Q',d,552,2),
               'backup-array-overlap':lambda d:struct.pack_into('<Q',d,560,total-2),
               'table-lba-overflow':lambda d:struct.pack_into('<Q',d,584,2**64-1),
               'partition-lba-overflow':lambda d:struct.pack_into('<Q',d,1056,2**64-1),
               'hybrid-mbr':lambda d:d.__setitem__(466,7)}
        for name,change in cases.items():
            with self.subTest(name=name):
                bad=self.disk.copy();change(bad);gpt_checksums(bad);self.image.write_bytes(bad);self.automatic('refused')
    def test_ambiguous_candidates_refused(self):
        bad=self.disk.copy();bad[1152:1280]=bad[1024:1152];bad[1168]^=1
        struct.pack_into('<QQ',bad,1184,20000,21023);gpt_checksums(bad);self.image.write_bytes(bad)
        r=self.automatic('refused');self.assertIn('workspace-candidate-count',r['reason'])
    def test_no_basic_candidate_refused(self):
        bad=self.disk.copy();bad[1024]^=1;gpt_checksums(bad);self.image.write_bytes(bad)
        self.automatic('refused')
    def test_divergent_valid_backup_refused(self):
        bad=self.disk.copy();total=len(bad)//512;at=(total-33)*512;bad[at+16]^=1
        h=(total-1)*512;struct.pack_into('<I',bad,h+88,zlib.crc32(bad[at:(total-1)*512]));struct.pack_into('<I',bad,h+16,0)
        struct.pack_into('<I',bad,h+16,zlib.crc32(bad[h:h+92]));self.image.write_bytes(bad)
        self.automatic('refused')
    def test_valid_crc_backup_header_mismatch_refused(self):
        for field in (24,32,40,56,80):
            with self.subTest(field=field):
                bad=self.disk.copy();at=len(bad)-512;bad[at+field]^=1
                struct.pack_into('<I',bad,at+16,0);struct.pack_into('<I',bad,at+16,zlib.crc32(bad[at:at+92]))
                self.image.write_bytes(bad);self.automatic('refused')
    def test_backup_bounds_refused(self):
        bad=self.disk.copy();at=len(bad)-512;struct.pack_into('<Q',bad,at+72,2048)
        struct.pack_into('<I',bad,at+16,0);struct.pack_into('<I',bad,at+16,zlib.crc32(bad[at:at+92]));self.image.write_bytes(bad)
        self.automatic('refused')
    def test_no_fallback_when_selector_fails(self):
        with mock.patch.object(inspect.native,'driver_output',side_effect=ValueError('selector refused')) as call:
            self.automatic('refused');self.assertEqual(call.call_count,1)
    def test_writable_native_fd_refused(self):
        before=snapshot(self.image);fd=os.open(self.image,os.O_RDWR)
        try:r=subprocess.run([str(GPT_DRIVER),'--fd',str(fd),'0',str(len(self.disk))],pass_fds=(fd,),capture_output=True)
        finally:os.close(fd)
        self.assertNotEqual(r.returncode,0);self.assertEqual(r.stdout,b'');self.assertEqual(snapshot(self.image),before)
    def test_incomplete_explicit_range_refused(self):
        for offset,length in ((0,None),(None,512),(2048*512,len(self.fs.data))):
            r=inspect.inspect_workspace(self.image,DRIVER,offset,length,gpt_driver=GPT_DRIVER)
            self.assertEqual(r['state'],'refused')
    def test_invalid_selection_report_refused(self):
        report=self.automatic()['gpt_selection'];report['offset_bytes']=len(self.disk)
        with mock.patch.object(inspect.native,'driver_output',return_value=report):self.automatic('refused')




FIRST_GUID='66666666-7777-8888-9999-aaaaaaaaaaaa'
SECOND_GUID='10203040-5060-7080-90a0-b0c0d0e0f001'
ABSENT_GUID='12345678-abcd-ef01-2345-6789abcdef01'

class GUIDWorkspaceInspectionTests(unittest.TestCase):
    def setUp(self):
        WorkspaceInspectionTests.setUp(self)
        other=empty_image(8*1024*1024,serial=0xfedcba98)
        other.add_directory_files('srvros-work',{'.workspace':b'SRVROS-WORKSPACE-V1 fedcba98\n','source':b'second-committed-value'})
        self.second=other.data
        self.disk=whole_disk(self.fs.data,20*1024*1024)
        self.disk[1152:1280]=self.disk[1024:1152]
        self.disk[1168:1184]=uuid.UUID(SECOND_GUID).bytes_le
        struct.pack_into('<QQ',self.disk,1184,20480,20480+len(self.second)//512-1)
        self.disk[20480*512:20480*512+len(self.second)]=self.second
        gpt_checksums(self.disk);self.image.write_bytes(self.disk)
    def guided(self,guid=FIRST_GUID,expected='inspected'):
        before=snapshot(self.image)
        r=inspect.inspect_workspace(self.image,DRIVER,gpt_driver=GPT_DRIVER,partition_guid=guid)
        self.assertEqual(r['state'],expected,r);self.assertEqual(snapshot(self.image),before)
        self.assertEqual(r['admission'],'not-authorized')
        if expected=='inspected':
            self.assertTrue(r['image_unchanged']);self.assertEqual(r['requested_partition_guid'],guid)
            self.assertEqual(r['gpt_selection']['partition_guid_le'],uuid.UUID(guid).bytes_le.hex())
        else:self.assertNotIn('diagnostic',r)
        return r
    def test_explicitly_selects_each_workspace(self):
        for guid,offset,marker in ((FIRST_GUID,2048*512,'1234abcd'),(SECOND_GUID,20480*512,'fedcba98')):
            with self.subTest(guid=guid):
                r=self.guided(guid);explicit=inspect.inspect_workspace(self.image,DRIVER,offset,len(self.fs.data))
                self.assertEqual(r['diagnostic'],explicit['diagnostic']);self.assertEqual(r['diagnostic']['marker_uuid'],marker)
                self.assertFalse(r['diagnostic']['marker_binding_authenticated'])
    def test_command_line_guid_and_refusals(self):
        before=snapshot(self.image)
        command=[sys.executable,str(ROOT/'tools/workspace_inspection.py'),str(self.image),'--driver',str(DRIVER),'--gpt-driver',str(GPT_DRIVER)]
        for extra,code in (([],1),(['--partition-guid',SECOND_GUID],0),(['--partition-guid',ABSENT_GUID],1)):
            with self.subTest(extra=extra):
                result=subprocess.run(command+extra,capture_output=True,text=True)
                self.assertEqual(result.returncode,code,result.stderr)
                report=json.loads(result.stdout);self.assertEqual(report['state'],'inspected' if code==0 else 'refused')
                if code==0:self.assertEqual(report['requested_partition_guid'],SECOND_GUID)
                self.assertEqual(snapshot(self.image),before)
    def test_automatic_ambiguity_stays_refused(self):
        for _ in range(2):
            before=snapshot(self.image);r=inspect.inspect_workspace(self.image,DRIVER,gpt_driver=GPT_DRIVER)
            self.assertEqual(r['state'],'refused');self.assertIn('workspace-candidate-count',r['reason'])
            self.assertEqual(snapshot(self.image),before);self.guided(SECOND_GUID)
    def test_wrong_guid_has_no_fallback(self):
        for data in (self.disk,whole_disk(self.fs.data)):
            self.image.write_bytes(data)
            with mock.patch.object(inspect.native,'driver_output',wraps=inspect.native.driver_output) as call:
                r=self.guided(ABSENT_GUID,'refused');self.assertEqual(call.call_count,1)
                self.assertIn('workspace-partition-guid',r['reason']);self.assertTrue(r['image_unchanged'])
    def test_disk_guid_is_not_a_partition_guid(self):
        self.guided('11111111-2222-3333-4444-555555555555','refused')
    def test_guid_for_non_basic_partition_refused(self):
        bad=self.disk.copy();bad[1152:1168]=uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b').bytes_le
        gpt_checksums(bad);self.image.write_bytes(bad);self.guided(SECOND_GUID,'refused')
    def test_selected_bad_workspace_does_not_fall_back(self):
        bad=self.disk.copy();bad[20480*512:20480*512+len(self.second)]=bytes(len(self.second));self.image.write_bytes(bad)
        self.guided(FIRST_GUID)
        with mock.patch.object(inspect.native,'driver_output',wraps=inspect.native.driver_output) as call:
            self.guided(SECOND_GUID,'refused');self.assertEqual(call.call_count,2)
    def test_unselected_overlap_refuses_whole_map(self):
        bad=self.disk.copy();struct.pack_into('<QQ',bad,1184,2048,4095);gpt_checksums(bad);self.image.write_bytes(bad)
        r=self.guided(FIRST_GUID,'refused');self.assertIn('primary-gpt',r['reason'])
    def test_duplicate_identity_refuses_whole_map(self):
        bad=self.disk.copy();bad[1168:1184]=bad[1040:1056];gpt_checksums(bad);self.image.write_bytes(bad)
        self.guided(FIRST_GUID,'refused')
    def test_backup_validation_precedes_guid_lookup(self):
        bad=self.disk.copy();bad[len(bad)-512+16]^=1;self.image.write_bytes(bad)
        for guid in (FIRST_GUID,ABSENT_GUID):
            r=self.guided(guid,'refused');self.assertIn('backup-gpt',r['reason'])
    def test_malformed_guid_refused(self):
        for guid in ('',FIRST_GUID.upper(),'00000000-0000-0000-0000-000000000000',FIRST_GUID.replace('-',''),'{'+FIRST_GUID+'}',FIRST_GUID+' ',42):
            with self.subTest(guid=guid):self.guided(guid,'refused')
    def test_guid_cannot_bypass_gpt_with_explicit_range(self):
        before=snapshot(self.image)
        r=inspect.inspect_workspace(self.image,DRIVER,2048*512,len(self.fs.data),partition_guid=FIRST_GUID)
        self.assertEqual(r['state'],'refused');self.assertEqual(snapshot(self.image),before)
    def test_native_guid_validation_and_readonly_fd(self):
        before=snapshot(self.image);fd=os.open(self.image,os.O_RDONLY)
        try:
            for value in ('','0'*32,'G'*32,FIRST_GUID,uuid.UUID(SECOND_GUID).bytes_le.hex().upper()):
                r=subprocess.run([str(GPT_DRIVER),'--fd',str(fd),'0',str(len(self.disk)),'--partition-guid-le',value],pass_fds=(fd,),capture_output=True)
                self.assertNotEqual(r.returncode,0);self.assertEqual(r.stdout,b'')
        finally:os.close(fd)
        fd=os.open(self.image,os.O_RDWR)
        try:r=subprocess.run([str(GPT_DRIVER),'--fd',str(fd),'0',str(len(self.disk)),'--partition-guid-le',uuid.UUID(SECOND_GUID).bytes_le.hex()],pass_fds=(fd,),capture_output=True)
        finally:os.close(fd)
        self.assertNotEqual(r.returncode,0);self.assertEqual(r.stdout,b'');self.assertEqual(snapshot(self.image),before)
    def test_native_wrong_guid_refused_with_single_candidate(self):
        self.image.write_bytes(whole_disk(self.fs.data));before=snapshot(self.image)
        fd=os.open(self.image,os.O_RDONLY)
        try:r=subprocess.run([str(GPT_DRIVER),'--fd',str(fd),'0',str(self.image.stat().st_size),'--partition-guid-le',uuid.UUID(ABSENT_GUID).bytes_le.hex()],pass_fds=(fd,),capture_output=True)
        finally:os.close(fd)
        self.assertNotEqual(r.returncode,0);self.assertEqual(r.stdout,b'');self.assertEqual(snapshot(self.image),before)
    def test_wrapper_rejects_wrong_selected_identity(self):
        report=self.guided(FIRST_GUID)['gpt_selection']
        with mock.patch.object(inspect.native,'driver_output',return_value=report) as call:
            self.guided(SECOND_GUID,'refused');self.assertEqual(call.call_count,1)
    def test_dirty_selected_workspace_and_readonly_permissions(self):
        struct.pack_into('<H',self.disk,20480*512+106,2);self.image.write_bytes(self.disk);self.image.chmod(0o444)
        r=self.guided(SECOND_GUID);self.assertTrue(r['diagnostic']['filesystem']['dirty']);self.assertEqual(r['image_mode_octal'],'0444')

if __name__=='__main__':unittest.main()
