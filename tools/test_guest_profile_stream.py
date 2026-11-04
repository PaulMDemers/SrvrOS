#!/usr/bin/env python3
"""Focused v2 guest tests against a compiled real-source profile executable."""
import hashlib
import json
import os
import resource
import signal
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())
MAGIC = b'SRVROS-PROFILE2\n'

class Stream(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='profile-stream-')
        self.root = Path(self.tmp.name).resolve()
        (self.root/'a').mkdir(); (self.root/'b').mkdir()
        self.data = bytes(range(256))*8193
        self.manifest = {'format':'org.srvros.portable-profile','schema_version':2,
            'profile':{'id':'12345678-1234-4234-8234-123456789abc','display_name':'Stream'},
            'preferences':{'text_size':16},'documents':[{'path':'Documents/data.bin',
            'size':len(self.data),'sha256':hashlib.sha256(self.data).hexdigest()}]}
        self.source = self.root/'source.bundle'; self.write_bundle()
    def tearDown(self): self.tmp.cleanup()
    def write_bundle(self):
        meta=json.dumps(self.manifest,separators=(',',':')).encode()
        self.source.write_bytes(MAGIC+struct.pack('<II',len(meta),0)+meta+self.data)
    def run_cli(self,*args,code=0,stop=None,io=None,nofile=None):
        env=dict(os.environ);env.pop('SRVROS_PROFILE_STOP_AFTER',None);env.pop('SRVROS_PROFILE_TEST_IO',None)
        if stop: env['SRVROS_PROFILE_STOP_AFTER']=stop
        if io: env['SRVROS_PROFILE_TEST_IO']=io
        def limit(): resource.setrlimit(resource.RLIMIT_NOFILE,(nofile,nofile))
        r=subprocess.run([BIN,*map(str,args)],env=env,capture_output=True,preexec_fn=limit if nofile else None)
        self.assertEqual(r.returncode,code,r.stderr.decode(errors='replace'));return r
    def test_target_request_adapter(self):
        root=Path(__file__).resolve().parents[1];src=self.root/'adapter.c';out=self.root/'adapter'
        src.write_text(r'''#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <srvros/file_stream.h>
#include <srvros/sys.h>
#include "stream.h"
static int operation,bad_stat;
void profile_memory_report(const char *p){(void)p;}
long srv_file_stream(const struct srv_file_stream_request *r){
    assert(r->version==1 && r->size==80 && r->operation==(unsigned)operation);
    assert(!r->flags && !r->reserved[0] && !r->reserved[1]);
    if(operation==1 || operation==2){assert(r->path && !r->handle && !r->buffer && !r->length && !r->offset);assert(r->mode==(operation==2?0600u:0u));return 4294967297LL;}
    assert(r->handle==4294967297ULL && !r->path && !r->mode);
    if(operation==3){assert(r->buffer && r->length==5 && r->offset==17);return 5;}
    if(operation==4){assert(r->buffer && r->length==5 && !r->offset);return 5;}
    if(operation==5){assert(r->buffer && r->length==112 && !r->offset);struct srv_file_stream_stat *s=(void *)(uintptr_t)r->buffer;s->abi_version=bad_stat==1?99:1;s->struct_size=bad_stat==2?0:112;s->type=bad_stat==3?1:0;s->size=7;s->inode=9;s->mode=0100600;s->nlink=1;return 0;}
    assert(!r->buffer && !r->length && !r->offset);return operation==7?-95:0;
}
long srv_file_change(const struct srv_file_change *r){assert(r->abi_version==1&&r->struct_size==sizeof(*r)&&r->operation==SRV_FILE_RENAME&&r->expected_inode==9&&r->expected_size==7&&r->expected_mtime==0&&r->expected_type==0&&!strcmp(r->path,"/fat/x")&&!strcmp(r->destination,"/fat/y"));return 0;}
int main(void){char b[5];struct profile_stream_info i;profile_stream_handle h;
    operation=1;h=profile_stream_open_read("/fat/x");assert(h==4294967297LL);
    operation=2;assert(profile_stream_open_new("/fat/x")==h);
    operation=3;assert(profile_stream_pread(h,b,5,17)==5);
    operation=4;assert(profile_stream_write(h,b,5)==5);
    operation=5;assert(!profile_stream_stat(h,&i)&&i.size==7&&i.inode==9&&i.nlink==1&&i.dev==1);
    for(bad_stat=1;bad_stat<=3;bad_stat++)assert(profile_stream_stat(h,&i)==-1&&errno==EIO);
    assert(!profile_stream_publish("/fat/x","/fat/y",&i));operation=6;assert(!profile_stream_sync(h));operation=7;assert(profile_stream_close(h)==-1&&errno==95);return 0;}
''')
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'shared/include'),
            '-I',str(root/'userspace/profile'),'-idirafter',str(root/'userspace/lib/include'),str(src),
            str(root/'userspace/profile/stream.c'),str(root/'userspace/profile/codec.c'),'-o',str(out)],check=True)
        subprocess.run([str(out)],check=True)

    def test_unsupported_backend_legacy_fallback(self):
        import base64
        payload=b'legacy'; legacy={'format':'org.srvros.portable-profile','schema_version':1,
            'profile':self.manifest['profile'],'preferences':{},'documents':[{'path':'Documents/old.txt',
            'size':len(payload),'sha256':hashlib.sha256(payload).hexdigest(),'data_base64':base64.b64encode(payload).decode()}]}
        old=self.root/'legacy.bundle';old.write_text(json.dumps(legacy))
        self.assertEqual(json.loads(self.run_cli('inspect',old,io='unsupported').stdout)['bytes'],len(payload))
        self.run_cli('inspect',self.source,code=1,io='unsupported')
        self.run_cli('preview-import',self.source,self.root/'a'/'bad','bad',code=1,io='unsupported')
        self.assertFalse((self.root/'a'/'bad').exists())

    def test_import_recovery_export(self):
        home=self.root/'a'/'alice';review=json.loads(self.run_cli('preview-import',self.source,home,'alice').stdout)
        self.assertEqual(review['bundle_version'],2); self.assertEqual(review['sha256'],hashlib.sha256(self.source.read_bytes()).hexdigest())
        d=review['destination'];self.run_cli('import-reviewed',self.source,home,'alice',d['uid'],d['gid'],review['sha256'],code=86,stop='copy-partial')
        self.run_cli('check-recover',home);self.run_cli('recover',home)
        self.assertEqual((home/'Documents/data.bin').read_bytes(),self.data)
        out=self.root/'out.bundle';exp=json.loads(self.run_cli('preview-export-auto',home,'Documents/data.bin').stdout)
        self.assertEqual(exp['bundle_version'],2)
        self.run_cli('export-auto-reviewed',home,out,exp['sha256'],'Documents/data.bin')
        self.assertEqual(hashlib.sha256(out.read_bytes()).hexdigest(),exp['sha256'])
        self.assertEqual(json.loads(self.run_cli('inspect',out).stdout)['bytes'],len(self.data))
        self.run_cli('import',out,self.root/'b'/'bob','bob',2001,2001)
        self.assertEqual((self.root/'b'/'bob'/'Documents/data.bin').read_bytes(),self.data)
    def test_all_recovery_checkpoints(self):
        for i,phase in enumerate(('journal','directories','document','identity','commit-ready','published')):
            self.manifest['profile']['id']=f'12345678-1234-4234-8234-{i:012x}';self.write_bundle()
            home=self.root/'a'/f'user{i}'
            self.run_cli('import',self.source,home,f'user{i}',1001+i,2001+i,code=86,stop=phase)
            self.run_cli('check-recover',home);self.run_cli('recover',home)
            self.assertEqual((home/'Documents/data.bin').read_bytes(),self.data)

    def test_auto_small_remains_v1(self):
        self.data=b'hello';self.manifest['documents'][0].update(size=5,sha256=hashlib.sha256(self.data).hexdigest());self.write_bundle()
        home=self.root/'a'/'small';self.run_cli('import',self.source,home,'small',1001,2001)
        review=json.loads(self.run_cli('preview-export-auto',home,'Documents/data.bin').stdout)
        self.assertEqual(review['bundle_version'],1)
        out=self.root/'small.bundle';self.run_cli('export-auto-reviewed',home,out,review['sha256'],'Documents/data.bin')
        self.assertEqual(json.loads(out.read_bytes())['schema_version'],1)

    def test_stream_io_failures_and_short_io(self):
        home=self.root/'a'/'alice'
        self.run_cli('preview-import',self.source,home,'alice',code=1,io='read-error')
        self.assertFalse(home.exists())
        self.run_cli('import',self.source,home,'alice',1001,2001,code=1,io='write-error')
        self.assertTrue(Path(str(home)+'.profile-import.json').exists())
        self.assertFalse((home/'profile.id').exists())
        self.run_cli('recover',home,io='read-short')
        out=self.root/'out.bundle';review=json.loads(self.run_cli('preview-export-auto',home,'Documents/data.bin').stdout)
        self.run_cli('export-auto-reviewed',home,out,review['sha256'],'Documents/data.bin',io='write-short')
        self.assertEqual(hashlib.sha256(out.read_bytes()).hexdigest(),review['sha256'])
        out2=self.root/'failed.bundle'
        self.run_cli('export-auto-reviewed',home,out2,review['sha256'],'Documents/data.bin',code=1,io='fsync-error')
        self.assertTrue(out2.exists())
        self.run_cli('export-auto-reviewed',home,out2,review['sha256'],'Documents/data.bin',code=1)

    def test_size_boundary_and_memory(self):
        block=b'x'*65536;docsize=16*1024*1024
        digest=hashlib.sha256();
        for _ in range(docsize//len(block)): digest.update(block)
        self.manifest['documents']=[{'path':f'Documents/data{i}.bin','size':docsize,'sha256':digest.hexdigest()} for i in range(4)]
        meta=json.dumps(self.manifest,separators=(',',':')).encode()
        with self.source.open('wb') as f:
            f.write(MAGIC+struct.pack('<II',len(meta),0)+meta)
            for _ in range(4*docsize//len(block)):f.write(block)
        home=self.root/'a'/'large'
        r=self.run_cli('preview-import',self.source,home,'large')
        self.assertEqual(json.loads(r.stdout)['bytes'],64*1024*1024)
        peaks=[int(line.split(b'owned_peak_bytes=')[1].split()[0]) for line in r.stderr.splitlines() if b'owned_peak_bytes=' in line]
        self.assertTrue(peaks);self.assertLess(max(peaks),1024*1024)
        def peak(result):
            values=[int(line.split(b'owned_peak_bytes=')[1].split()[0]) for line in result.stderr.splitlines() if b'owned_peak_bytes=' in line]
            self.assertTrue(values);self.assertLess(max(values),1024*1024)
        peak(self.run_cli('import',self.source,home,'large',1001,2001,code=86,stop='copy-partial'))
        peak(self.run_cli('check-recover',home));peak(self.run_cli('recover',home))
        for doc in self.manifest['documents']:
            with (home/doc['path']).open('rb') as f: self.assertEqual(hashlib.file_digest(f,'sha256').hexdigest(),doc['sha256'])
        names=[doc['path'] for doc in self.manifest['documents']]
        review=self.run_cli('preview-export-auto',home,*names);peak(review)
        fingerprint=json.loads(review.stdout)['sha256'];out=self.root/'large.bundle'
        peak(self.run_cli('export-auto-reviewed',home,out,fingerprint,*names))
        with out.open('rb') as f:self.assertEqual(hashlib.file_digest(f,'sha256').hexdigest(),fingerprint)
        self.manifest['documents'][0]['size']=docsize+1;self.write_bundle()
        self.run_cli('inspect',self.source,code=1)
    def test_export_rejects_source_mutation_and_replacement_during_copy(self):
        home=self.root/'a'/'alice';self.run_cli('import',self.source,home,'alice',1001,2001)
        doc=home/'Documents/data.bin'
        for kind in ('mutation','replacement'):
            review=json.loads(self.run_cli('preview-export-auto',home,'Documents/data.bin').stdout)
            output=self.root/(kind+'.bundle');env=dict(os.environ,SRVROS_PROFILE_TEST_EXPORT_PAUSE='1')
            proc=subprocess.Popen([BIN,'export-auto-reviewed',str(home),str(output),review['sha256'],'Documents/data.bin'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            pid,status=os.waitpid(proc.pid,os.WUNTRACED);self.assertEqual(pid,proc.pid);self.assertTrue(os.WIFSTOPPED(status))
            if kind=='mutation':
                with doc.open('r+b') as f:f.seek(42);f.write(b'!')
            else:
                other=doc.with_name('replacement.tmp');other.write_bytes(doc.read_bytes());os.replace(other,doc)
            os.kill(proc.pid,signal.SIGCONT);stdout,stderr=proc.communicate(timeout=60)
            self.assertNotEqual(proc.returncode,0,(stdout,stderr));self.assertTrue(output.exists())
            doc.write_bytes(self.data)

    def test_created_scratch_and_export_identity_replacement(self):
        home=self.root/'a'/'alice'
        env=dict(os.environ,SRVROS_PROFILE_TEST_SCRATCH_PAUSE='1')
        proc=subprocess.Popen([BIN,'import',str(self.source),str(home),'alice','1001','2001'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        pid,status=os.waitpid(proc.pid,os.WUNTRACED);self.assertEqual(pid,proc.pid);self.assertTrue(os.WIFSTOPPED(status))
        scratch=home/'.profile-copy-temp';replacement=home/'replacement.tmp';replacement.write_bytes(scratch.read_bytes());os.replace(replacement,scratch)
        os.kill(proc.pid,signal.SIGCONT);stdout,stderr=proc.communicate(timeout=60)
        self.assertNotEqual(proc.returncode,0,(stdout,stderr));self.assertFalse((home/'Documents/data.bin').exists())
        self.run_cli('recover',home)
        review=json.loads(self.run_cli('preview-export-auto',home,'Documents/data.bin').stdout);output=self.root/'output.bundle'
        env=dict(os.environ,SRVROS_PROFILE_TEST_OUTPUT_PAUSE='1')
        proc=subprocess.Popen([BIN,'export-auto-reviewed',str(home),str(output),review['sha256'],'Documents/data.bin'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        pid,status=os.waitpid(proc.pid,os.WUNTRACED);self.assertEqual(pid,proc.pid);self.assertTrue(os.WIFSTOPPED(status))
        replacement=self.root/'output-replacement';replacement.write_bytes(output.read_bytes());os.replace(replacement,output)
        os.kill(proc.pid,signal.SIGCONT);stdout,stderr=proc.communicate(timeout=60)
        self.assertNotEqual(proc.returncode,0,(stdout,stderr));self.assertTrue(output.exists())

    def test_64_source_descriptor_limit_and_export_partial(self):
        small=b'z';large=b'q'*(1024*1024+1)
        docs=[{'path':f'Documents/file{i:02d}.bin','size':len(large if i==0 else small),
               'sha256':hashlib.sha256(large if i==0 else small).hexdigest()} for i in range(64)]
        self.manifest['documents']=docs;self.data=large+small*63;self.write_bundle()
        home=self.root/'a'/'many';self.run_cli('import',self.source,home,'many',1001,2001)
        names=[d['path'] for d in docs]
        preview=json.loads(self.run_cli('preview-export-auto',home,*names,nofile=67).stdout)
        self.assertEqual(preview['bundle_version'],2)
        partial=self.root/'partial.bundle'
        self.run_cli('export-auto-reviewed',home,partial,preview['sha256'],*names,code=86,stop='export-partial',nofile=67)
        self.assertTrue(partial.read_bytes().startswith(MAGIC));self.run_cli('inspect',partial,code=1)
        out=self.root/'many.bundle';self.run_cli('export-auto-reviewed',home,out,preview['sha256'],*names,nofile=67)
        with out.open('rb') as f:self.assertEqual(hashlib.file_digest(f,'sha256').hexdigest(),preview['sha256'])

    def test_max_document_review_fits_gui(self):
        payload=b'X';digest=hashlib.sha256(payload).hexdigest()
        self.manifest['documents']=[{'path':'Documents/'+('a'*14)+f'{i:02d}/'+('b'*14)+f'{i:02d}.txt','size':1,'sha256':digest} for i in range(64)]
        self.data=payload*64;self.write_bundle()
        r=self.run_cli('preview-import',self.source,self.root/'a'/'max','max')
        self.assertEqual(len(json.loads(r.stdout)['documents']),64)
        self.assertLessEqual(len(r.stdout),16384)
        peaks=[int(line.split(b'owned_peak_bytes=')[1].split()[0]) for line in r.stderr.splitlines() if b'owned_peak_bytes=' in line]
        self.assertLess(max(peaks),1024*1024)
    def test_malformed_and_review_binding(self):
        home=self.root/'a'/'alice';review=json.loads(self.run_cli('preview-import',self.source,home,'alice').stdout)
        raw=self.source.read_bytes();self.source.write_bytes(raw[:-1]+bytes([raw[-1]^1]))
        d=review['destination'];self.run_cli('import-reviewed',self.source,home,'alice',d['uid'],d['gid'],review['sha256'],code=1)
        self.assertFalse(home.exists())
        for bad in (raw+b'x',raw[:-1],raw[:20]+b'\1\0\0\0'+raw[24:]):
            self.source.write_bytes(bad);self.run_cli('inspect',self.source,code=1)

if __name__=='__main__': unittest.main()
