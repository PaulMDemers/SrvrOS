#!/usr/bin/env python3
"""Production private-append proofs and real process/VFS/exFAT overlap replay.

Controls and candidate execute the SAME 65 foreign global sync calls and 65
other-file markers. Timing/controller/scheduler seams are host models, not
native acceptance. Source-barrier I/O excludes independent validation reads.
"""
import argparse
import hashlib
import gzip
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
from exfat_test_image import ExfatFixture
from test_storage_leases import empty_image

ROOT=Path(__file__).resolve().parents[1]
BYTES=1366*768*4+54


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'build/review/reserved-append')
    p.add_argument('--limine-include',type=Path,default=ROOT/'build/limine-protocol/include')
    p.add_argument('--compile-only',action='store_true')
    args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    record={'status':'RUNNING','scope':__doc__,'commands':[],'cases':[]}
    (out/'result.json').write_text(json.dumps(record,indent=2)+'\n')
    fsck=shutil.which('fsck.exfat')
    if not fsck:p.error('exfatprogs fsck.exfat is required; no media checks are skipped')
    def run(command,name,env=None):
        proc=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=180,env={**os.environ,**(env or {})})
        (out/(name+'.log')).write_text(proc.stdout+proc.stderr)
        assert proc.returncode==0,(name,proc.returncode,proc.stdout,proc.stderr)
        return proc.stdout
    def compile_case(name,sources,extra=()):
        command=shlex.split(os.environ.get('CC','cc'))
        if platform.system()=='Darwin':command+=['-arch','x86_64']
        command+=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',
                  '-idirafter',str(ROOT/'userspace/lib/include'),'-I',str(ROOT/'shared/include'),
                  '-I',str(ROOT/'kernel/include'),'-I',str(args.limine_include.resolve()),*extra]
        command += [str(ROOT/source) for source in sources]
        binary=out/name;command += ['-Wl,-dead_strip' if platform.system()=='Darwin' else '-Wl,--gc-sections','-o',str(binary)]
        record['commands'].append(command);run(command,name+'-build');return binary
    unit=compile_case('reserved-unit',['tools/test_reserved_append.c','tools/test_process_teardown_fault.c',
        'kernel/src/vfs_namespace.c','kernel/src/pty.c','kernel/src/pty_model.c'],['-DSRVROS_PTY_HOST_TEST'])
    irq=compile_case('reserved-irq',['tools/test_reserved_append_irq.c'])
    queued=compile_case('reserved-queued',['tools/test_reserved_append_wait.c','tools/test_process_teardown_fault.c',
        'kernel/src/vfs_namespace.c','kernel/src/pty.c','kernel/src/pty_model.c'],['-DSRVROS_PTY_HOST_TEST'])
    backend=compile_case('reserved-backend',['tools/test_reserved_append_driver.c','tools/test_reserved_append_process.c',
        'kernel/src/fs/exfat.c','kernel/src/vfs.c','kernel/src/vfs_namespace.c','kernel/src/autotest_storage.c','kernel/src/file_verify.c'],
        ['-DSRVROS_EXFAT_HOST_TEST','-DSRVROS_EXFAT_COST_TEST','-DSRVROS_SYNC_CONTENT_HOST_TEST'])
    if args.compile_only:
        record['status']='COMPILED_NOT_TESTED';(out/'result.json').write_text(json.dumps(record,indent=2)+'\n');return
    run([str(unit)],'unit');run([str(irq)],'irq');run([str(queued)],'queued')
    fixture=empty_image(32*1024*1024,cluster_shift=8)
    fixture.add_directory_files('work',{'other.txt':b'','anchor.txt':b'unchanged'})
    seed=bytes(fixture.data);image=out/'work.exfat';rows={};mode_text={}
    expected=bytes((i*37+11)&255 for i in range(BYTES))
    for mode in ('baseline','sequential','overlap'):
        image.write_bytes(seed);text=run([str(backend),str(image),mode],mode);mode_text[mode]=text
        match=re.search(r'reserved-result: mode=(\w+) writes=(\d+) bytes=(\d+) foreign_syncs=(\d+) fast=(\d+) ordinary=(\d+) misses=(\d+) snapshots=(\d+) snapshot_bytes=(\d+) old_prefix_bytes=(\d+) fresh_verifications=2 allocation_clean=1',text)
        assert match and match[1]==mode
        row=dict(zip(('writes','bytes','foreign_syncs','fast','ordinary','misses','snapshots','snapshot_bytes','old_prefix_bytes'),map(int,match.groups()[1:])))
        assert row['writes']==65 and row['bytes']==BYTES and row['foreign_syncs']==65
        assert row['ordinary']+row['fast']==65
        barriers=[]
        for line in text.splitlines():
            if not line.startswith('reserved-barrier:'):continue
            fields={key:int(value) for key,value in re.findall(r'(\w+)=(\d+)',line)}
            assert fields['round']==len(barriers)+1
            if fields['prefix_valid']:
                assert fields['committed']==fields['captured']
                assert fields['end_generation']-fields['start_generation']==fields['append_calls']
                assert fields['live_at_end']-fields['captured']==fields['append_bytes']<=262144
                assert fields['dirty_at_end']==bool(fields['append_bytes'])
            else:
                assert not fields['dirty_at_end'] and fields['committed']==BYTES
            barriers.append(fields)
        assert len(barriers)==65 and barriers[-1]['committed']==BYTES and not barriers[-1]['dirty']
        source=re.search(r'reserved-source-io: callbacks=(\d+) reads=(\d+) read_bytes=(\d+) writes=(\d+) write_bytes=(\d+) flushes=(\d+) prefix_read_bytes=(\d+)',text);assert source
        row['source_io']=dict(zip(('callbacks','reads','read_bytes','writes','write_bytes','flushes','prefix_read_bytes'),map(int,source.groups())))
        assert row['source_io']['prefix_read_bytes']==row['old_prefix_bytes']
        raw=image.read_bytes();state=ExfatFixture(raw)
        assert state.cluster==131072 and state.read('work/capture.bmp')==expected
        assert state.read('work/other.txt')==b'A'*17 and state.read('work/anchor.txt')==b'unchanged'
        check=subprocess.run([fsck,'-n',str(image)],capture_output=True,text=True)
        report=check.stdout+check.stderr;(out/(mode+'-fsck.log')).write_text(report)
        assert not check.returncode and ': clean.' in report and 'ERROR' not in report and 'corrupted' not in report
        row['payload_sha256']=hashlib.sha256(expected).hexdigest();row['barriers']=barriers
        row['allocation']='actual C full traversal + independent fsck clean'
        archive=out/(mode+'.exfat.gz')
        archive.write_bytes(gzip.compress(raw,mtime=0))
        assert gzip.decompress(archive.read_bytes())==raw
        row['image']={'path':archive.name,'raw_sha256':hashlib.sha256(raw).hexdigest(),
                      'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
        rows[mode]=row
    baseline,control,candidate=[rows[name] for name in ('baseline','sequential','overlap')]
    assert not baseline['fast'] and not control['fast'] and baseline['snapshots']==control['snapshots']==65
    assert baseline['source_io']==control['source_io'],'Opt-in reservation changed ordinary sequential I/O'
    assert candidate['fast']==52 and candidate['ordinary']==13 and candidate['snapshots']==14
    assert candidate['old_prefix_bytes']<baseline['old_prefix_bytes']
    assert candidate['source_io']['read_bytes']<baseline['source_io']['read_bytes']
    assert candidate['source_io']['callbacks']<baseline['source_io']['callbacks']
    assert all(rows[name]['foreign_syncs']==65 for name in rows)
    # Inject failures into the ACTUAL second target operation, after another
    # prefix already committed and while four accepted fast chunks are live.
    targets=[]
    for line in mode_text['overlap'].splitlines():
        if line.startswith('reserved-target-io:'):
            targets.append({key:int(value) for key,value in re.findall(r'(\w+)=(\d+)',line)})
    payload_cut=next(row['absolute'] for row in targets if row['barrier']==2 and row['write']==1 and row['role']==2)
    final_flush_cut=[row['absolute'] for row in targets if row['barrier']==2 and not row['write']][-1]
    faults=[]
    for kind,number,key in [('payload',payload_cut,'SRVROS_RESERVED_FAIL_WRITE'),('last-flush',final_flush_cut,'SRVROS_RESERVED_FAIL_FLUSH')]:
        image.write_bytes(seed);text=run([str(backend),str(image),'failure'],'failure-'+kind,{key:str(number)})
        match=re.search(r'reserved-failure: foreign_syncs=(\d+) captured=(\d+) prior_committed=(\d+) live=(\d+) media=(\d+) retained_read=(\d+) fast=(\d+) ordinary=(\d+) close_error=1 quarantined=1 allocation_ok=(\d+) leaks=(\d+) errors=(\d+)',text)
        assert match,'Actual backend failure was not exercised'
        fault=dict(zip(('foreign_syncs','captured','prior_committed','live','media','retained_read','fast','ordinary','allocation_ok','leaks','errors'),map(int,match.groups())))
        assert fault['foreign_syncs']==2 and fault['captured']==6*65536 and fault['prior_committed']==65536
        assert fault['live']==fault['retained_read']==10*65536 and fault['fast']==8 and fault['ordinary']==2
        assert fault['prior_committed']<=fault['media']<=fault['captured']
        raw=image.read_bytes();state=ExfatFixture(raw)
        assert state.read('work/capture.tmp')==expected[:fault['media']] and state.read('work/anchor.txt')==b'unchanged'
        assert state.read('work/other.txt')==bytes([1])*17
        (out/('failure-'+kind+'.exfat.gz')).write_bytes(gzip.compress(raw,mtime=0))
        check=subprocess.run([fsck,'-n',str(image)],capture_output=True,text=True)
        (out/('failure-'+kind+'-fsck.log')).write_text(check.stdout+check.stderr)
        fault.update(kind=kind,cut=number,raw_sha256=hashlib.sha256(raw).hexdigest(),fsck_exit=check.returncode,
                     status='EXPECTED_BACKEND_FAILURE_RAM_RETAINED_NOT_A_WORKLOAD_OR_CLEAN_RECOVERY_PASS')
        faults.append(fault)
    record['faults']=faults
    deps=['kernel/src/process.c','kernel/include/srvros/process.h','kernel/src/fs/exfat.c','kernel/src/fs/exfat_stream.inc',
          'kernel/src/sync_timing.c','kernel/include/srvros/sync_timing.h','kernel/src/capture_write_timing_state.h',
          'tools/test_reserved_append.c','tools/test_reserved_append_irq.c','tools/test_reserved_append_driver.c',
          'tools/test_reserved_append_process.c','tools/test_reserved_append_io.h','tools/test_reserved_append.py',
          'tools/test_sync_content_bridge.inc','tools/exfat_host_driver.c']
    record.update(status='PASS',cases=rows,source_sha256={path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in deps},
        limitation='Controlled host callback overlap and deterministic I/O counts only; not CPU/USB time or native lifecycle acceptance. Existing checked capture-save syscall integration remains separately tested.')
    (out/'result.json').write_text(json.dumps(record,indent=2)+'\n')
    print('reserved-append: PASS unit/IF1 teardown, identical sequential control, 65 foreign barriers each; overlap52fast+13ordinary, captured-prefix invariants, two actual fresh verifications, clean full allocation/media, measured source-I/O reduction and actual payload/final-flush failure RAM retention')


if __name__=='__main__':main()
