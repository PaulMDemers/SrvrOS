#!/usr/bin/env python3
"""Real exFAT adjacent-growth proofs, exact barriers, failure cuts and process sync.

Deterministic callback counts are not native USB timings or speed acceptance.
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
import struct
import subprocess

from append_contiguity import append_contiguity
from exfat_test_image import ExfatFixture
from test_storage_leases import empty_image

ROOT = Path(__file__).resolve().parents[1]
BYTES = 1366 * 768 * 4 + 54


def check_clean_image(path, fsck):
    process=subprocess.run([fsck,'-n',str(path)],capture_output=True,text=True)
    output=process.stdout+process.stderr
    if process.returncode or 'ERROR' in output or 'corrupted' in output or ': clean.' not in output:
        raise RuntimeError(output)
    return output


def save_raw(path, raw):
    """Losslessly retain disposable fault media without exhausting the host."""
    path.with_suffix(path.suffix+'.gz').write_bytes(gzip.compress(raw,mtime=0))
    return hashlib.sha256(raw).hexdigest()


def entry_of(fixture, name):
    return dict(fixture.entries(fixture.chain(fixture.root)))[name]


def root_seed(layout, inside=False):
    """Explicit topology: the positive seed allocates anchor BEFORE the target."""
    f = empty_image(16 * 1024 * 1024, cluster_shift=8)
    root = f.offset(f.root)
    bitmap = f.offset(struct.unpack_from('<I', f.data, root + 20)[0])
    cursor = 0
    while f.data[root + cursor]:
        cursor += 32
    free = [n for n in range(2, f.count + 2)
            if not f.data[bitmap + (n-2)//8] & (1 << ((n-2)%8))]
    def add(name, payload, chain=None, contiguous=True):
        nonlocal cursor, free
        if chain is None:
            chain = free[:(len(payload)+f.cluster-1)//f.cluster]
        free = [n for n in free if n not in chain]
        for i, n in enumerate(chain):
            f.data[bitmap+(n-2)//8] |= 1 << ((n-2)%8)
            struct.pack_into('<I', f.data, f.fat+n*4, chain[i+1] if i+1<len(chain) else 0xffffffff)
            start=f.offset(n); chunk=payload[i*f.cluster:(i+1)*f.cluster]
            f.data[start:start+len(chunk)]=chunk
        record=f.checked_entry(name,chain[0] if chain else 0,payload,contiguous=contiguous)
        f.data[root+cursor:root+cursor+len(record)]=record;cursor+=len(record)
        return chain
    old_size=2*f.cluster-(512 if inside else 0)
    old=bytes((i*17+37)&255 for i in range(old_size))
    if layout != 'nonadjacent':
        add('anchor.txt',b'unchanged')
    chain=add('keep.txt',old,contiguous=layout!='old-fat')
    if layout == 'nonadjacent':
        add('anchor.txt',b'unchanged')
    if layout == 'fragmented':
        # Every other remaining cluster belongs to a real FAT-backed file;
        # enough free clusters remain, but no four-cluster extent exists.
        occupied=free[1::2]
        add('filler.bin',bytes([0xa5])*(len(occupied)*f.cluster),occupied,False)
    if layout == 'alias':
        record=f.checked_entry('alias.txt',chain[0],old,contiguous=True)
        f.data[root+cursor:root+cursor+len(record)]=record
    return bytes(f.data), old, [(f.offset(n), min(f.cluster,len(old)-i*f.cluster)) for i,n in enumerate(chain)]


def trace(text, fixture, omit_fat=False):
    events=[]
    for line in text.splitlines():
        write=re.fullmatch(r'write-trace: call=(\d+) offset=(\d+) bytes=(\d+)',line)
        read=re.fullmatch(r'contiguous-read: round=(\d+) role=(\d+) offset=(\d+) bytes=(\d+) ok=(\d+)',line)
        if write:
            _,off,n=map(int,write.groups());kind='write'
        elif read:
            _,_,off,n,ok=map(int,read.groups());kind='read'
            assert ok==1
        elif line.startswith('flush-trace:'):
            events.append(('flush',));continue
        else:
            continue
        is_fat=fixture.fat<=off and off+n<=fixture.fat+(fixture.count+2)*4
        if not (omit_fat and is_fat):events.append((kind,off,n))
    return events


def compile_driver(out, name, exfat, limine, flags=()):
    executable=out/name
    command=shlex.split(os.environ.get('CC','cc'))
    if platform.system()=='Darwin':command+=['-arch','x86_64']
    command+=['-DSRVROS_EXFAT_HOST_TEST','-DSRVROS_EXFAT_COST_TEST','-DSRVROS_SYNC_CONTENT_HOST_TEST',
              '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',
              '-idirafter',str(ROOT/'userspace/lib/include'),'-I',str(ROOT/'shared/include'),
              '-I',str(ROOT/'kernel/include'),'-I',str(ROOT/'kernel/src/fs'),'-I',str(limine.resolve())]
    sources=[ROOT/'tools/test_contiguous_append_driver.c',ROOT/'tools/test_contiguous_append_process.c',
             exfat,ROOT/'kernel/src/vfs.c',ROOT/'kernel/src/vfs_namespace.c',ROOT/'kernel/src/autotest_storage.c']
    command+=list(flags)+list(map(str,sources))+['-Wl,-dead_strip' if platform.system()=='Darwin' else '-Wl,--gc-sections','-o',str(executable)]
    subprocess.run(command,check=True,cwd=ROOT)
    return executable,command


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/review/contiguous-append')
    parser.add_argument('--baseline-exfat',type=Path)
    parser.add_argument('--limine-include',type=Path,default=ROOT/'build/limine-protocol/include')
    parser.add_argument('--skip-faults',action='store_true',help='Only a development smoke; not the full regression')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    image=out/'work.exfat';result={'status':'RUNNING','scope':__doc__,'cases':[],'faults':[],'commands':[]}
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    fsck=shutil.which('fsck.exfat')
    if not fsck:parser.error('exfatprogs fsck.exfat is required for strict media and fault-repair checks')
    drivers={}
    baseline=args.baseline_exfat.resolve() if args.baseline_exfat else ROOT/'kernel/src/fs/exfat.c'
    for name,source,flags in [('candidate',ROOT/'kernel/src/fs/exfat.c',()),
                              ('baseline',baseline,() if args.baseline_exfat else ('-DSRVROS_EXFAT_CONTIGUOUS_APPEND_OFF',))]:
        drivers[name],command=compile_driver(out,name,source,args.limine_include,flags)
        result['commands'].append(command)
    def run(driver,mode,*numbers,allowed=(0,),env=None):
        process=subprocess.run([str(driver),str(image),mode,*map(str,numbers)],capture_output=True,text=True,
            timeout=120,env={**os.environ,'SRVROS_TRACE_WRITES':'1',**(env or {})})
        assert process.returncode in allowed,(mode,numbers,process.returncode,process.stdout,process.stderr)
        return process
    def allocation(driver):
        process=run(driver,'allocation-check',allowed=(0,1))
        match=re.search(r'allocation-check: (PASS|FAIL) files=([0-9]+) clusters=([0-9]+) leaked=([0-9]+) fat_bitmap_mismatches=([0-9]+) errors=([0-9]+)',process.stdout)
        assert match,'Missing strict full allocation traversal'
        values=dict(zip(('files','clusters','leaked','fat_bitmap_mismatches','errors'),map(int,match.groups()[1:])))
        values['status']=match[1]
        assert (process.returncode==0)==(values['status']=='PASS')
        return values,process.stdout
    def baseline_boundary(candidate_text,baseline_text,number):
        def writes(text):
            return [tuple(map(int,m.groups())) for m in re.finditer(r'write-trace: call=([0-9]+) offset=([0-9]+) bytes=([0-9]+)',text)]
        candidate=writes(candidate_text);target=next(row for row in candidate if row[0]==number)
        ordinal=sum(row[1:]==target[1:] for row in candidate if row[0]<=number)
        matching=[row for row in writes(baseline_text) if row[1:]==target[1:]]
        assert len(matching)>=ordinal,'Missing logically corresponding baseline write'
        return matching[ordinal-1][0]
    for layout in ('adjacent','nonadjacent','old-fat','fragmented','alias','changed-prefix'):
        for inside in ((False,True) if layout=='adjacent' else (False,)):
            seed,old,extents=root_seed('adjacent' if layout=='changed-prefix' else layout,inside)
            initial=ExfatFixture(seed);first=struct.unpack_from('<I',entry_of(initial,'keep.txt'),52)[0]
            mode='append-changed' if layout=='changed-prefix' else 'append-step'
            rows={}
            for name,driver in drivers.items():
                image.write_bytes(seed);process=run(driver,mode);text=process.stdout
                (out/f'{layout}-{inside}-{name}.log').write_text(text+process.stderr)
                raw=image.read_bytes();f=ExfatFixture(raw);entry=entry_of(f,'keep.txt')
                expected=bytes((i*17+37)&255 for i in range(5*f.cluster+512))
                if layout=='changed-prefix':expected=bytes([expected[0]^1])+expected[1:]
                assert f.read('keep.txt')==expected and f.read('anchor.txt')==b'unchanged'
                if layout=='alias':assert f.read('alias.txt')==old
                check_clean_image(image,fsck);run(driver,'allocation-check')
                counts=re.search(r'operation=PASS writes=(\d+) flushes=(\d+)',text);assert counts
                rows[name]={'writes':int(counts[1]),'flushes':int(counts[2]),'no_fat_chain':bool(entry[33]&2),
                    'first':struct.unpack_from('<I',entry,52)[0],'trace':trace(text,initial),
                    'nonfat_trace':trace(text,initial,True),'payload_sha256':hashlib.sha256(f.read('keep.txt')).hexdigest()}
                if name=='candidate':
                    observation=append_contiguity([text.encode()],require_exercised=layout=='adjacent')
                    rows[name]['contiguous_counters']=observation
                    if layout=='adjacent':assert observation['prefix_bytes']>=len(old)
                if layout=='adjacent' and name=='candidate':
                    assert rows[name]['no_fat_chain'] and rows[name]['first']==first,'Positive predicate not exercised'
                    assert all(raw[at:at+n]==seed[at:at+n] for at,n in extents)
                    # New allocation FAT initialization remains; only the now-
                    # irrelevant linking writes may be omitted.
                    new_count=(len(expected)+f.cluster-1)//f.cluster
                    assert all(f.u32(f.fat+(first+n)*4)==0xffffffff for n in range(2,new_count))
                elif layout in ('nonadjacent','old-fat','fragmented'):
                    assert not rows[name]['no_fat_chain'],'Negative path converted to NoFatChain'
                elif layout in ('alias','changed-prefix'):
                    assert rows[name]['first']!=first,'Private replacement fallback missing'
            if 'baseline' in rows:
                a,b=rows['candidate'],rows['baseline']
                assert a['payload_sha256']==b['payload_sha256'] and a['flushes']==b['flushes']
                assert a['nonfat_trace']==b['nonfat_trace'],'Non-FAT operations or flush order changed'
                if layout=='adjacent':
                    assert a['writes']<b['writes'] and not b['no_fat_chain']
                else:
                    assert a['trace']==b['trace'],'Negative path changed ordered callbacks'
            result['cases'].append({'layout':layout,'inside_cluster':inside,'results':rows})
            # All original prefix reads remain mandatory; a failed proof must
            # refuse before any mutation, even in the newly positive topology.
            if layout=='adjacent':
                image.write_bytes(seed);process=run(drivers['candidate'],'append-step',env={'SRVROS_FAIL_READ_OFFSET':str(extents[0][0])})
                assert 'operation=FAIL' in process.stdout and image.read_bytes()==seed
                if not args.skip_faults:
                    for kind,count in [('write-fail',rows['candidate']['writes']),('write-cut',rows['candidate']['writes']),
                                       ('tear',rows['candidate']['writes']),('flush-fail',rows['candidate']['flushes']),('flush-cut',rows['candidate']['flushes'])]:
                        for number in range(1,count+1):
                            (out/'current-cut.json').write_text(json.dumps(dict(layout=layout,inside_cluster=inside,kind=kind,number=number))+'\n')
                            image.write_bytes(seed);numbers=[0]*5
                            numbers[{'write-fail':0,'write-cut':1,'flush-fail':2,'flush-cut':3,'tear':4}[kind]]=number
                            process=run(drivers['candidate'],'append-step',*numbers,allowed=(0,77,78,79))
                            raw=image.read_bytes();assert all(raw[at:at+n]==seed[at:at+n] for at,n in extents),(kind,number,'committed payload overwritten')
                            assert ExfatFixture(raw).read('anchor.txt')==b'unchanged'
                            local_complete=None
                            if kind in ('write-fail','flush-fail'):
                                assert 'operation=FAIL' in process.stdout,'Fault was not exercised'
                                observation=append_contiguity([process.stdout.encode()])
                                local_complete=observation['local_complete']
                            # Reuse existing production-driver refusal check for
                            # dirty/quarantined media; clean-before/after cuts
                            # are legitimately importable and are not failures.
                            if struct.unpack_from('<H',raw,106)[0]&2:
                                run(drivers['candidate'],'inspect-cut')
                            repair=subprocess.run([fsck,'-y',str(image)],capture_output=True,text=True)
                            (out/'last-repair.log').write_text(repair.stdout+repair.stderr)
                            final=ExfatFixture(image.read_bytes());lost=False
                            try:assert final.read('keep.txt').startswith(old)
                            except StopIteration:lost=True
                            assert not lost or kind=='tear','Unexpected loss of committed filename'
                            if lost:
                                original_entry=entry_of(initial,'keep.txt')
                                entry_at=seed.find(original_entry,initial.offset(initial.root),initial.offset(initial.root)+initial.cluster)
                                publication=[int(m[1]) for m in re.finditer(r'write-trace: call=([0-9]+) offset=([0-9]+) bytes=([0-9]+)',(out/f'{layout}-{inside}-candidate.log').read_text()) if int(m[2])==entry_at]
                                assert kind=='tear' and number in publication,'Unexpected publication tear boundary'
                                repaired=image.read_bytes()
                            if lost and 'baseline' in drivers:
                                base_text=(out/f'{layout}-{inside}-baseline.log').read_text()
                                boundary=next(int(m[1]) for m in re.finditer(r'write-trace: call=([0-9]+) offset=([0-9]+) bytes=([0-9]+)',base_text) if int(m[2])==entry_at)
                                image.write_bytes(seed);run(drivers['baseline'],'append-step',0,0,0,0,boundary,allowed=(79,))
                                subprocess.run([fsck,'-y',str(image)],capture_output=True,text=True)
                                try:ExfatFixture(image.read_bytes()).read('keep.txt')
                                except StopIteration:pass
                                else:raise AssertionError('Baseline did not reproduce torn-name limitation')
                                image.write_bytes(repaired)
                            assert all(image.read_bytes()[at:at+n]==seed[at:at+n] for at,n in extents)
                            assert final.read('anchor.txt')==b'unchanged';check_clean_image(image,fsck)
                            allocation_result,allocation_log=allocation(drivers['candidate'])
                            baseline_allocation=None
                            if allocation_result['status']=='FAIL':
                                # Keep strict checker failures visible. exfatprogs
                                # may clear dirty flags without reclaiming every
                                # unpublished allocation. This is accepted only
                                # when the corresponding old algorithm has the
                                # same failed-repair result, never called clean.
                                failure=out/f'failed-cut-{inside}-{kind}-{number}'
                                failure.mkdir(exist_ok=True)
                                candidate_before=save_raw(failure/'before-fsck.exfat',raw)
                                repaired=image.read_bytes();candidate_after=save_raw(failure/'after-fsck.exfat',repaired)
                                (failure/'allocation.log').write_text(allocation_log)
                                (failure/'fsck.log').write_text(repair.stdout+repair.stderr)
                                (failure/'operation.log').write_text(process.stdout+process.stderr)
                                baseline_numbers=list(numbers)
                                if kind in ('write-fail','write-cut','tear'):
                                    mapped=baseline_boundary((out/f'{layout}-{inside}-candidate.log').read_text(),(out/f'{layout}-{inside}-baseline.log').read_text(),number)
                                    baseline_numbers[{'write-fail':0,'write-cut':1,'tear':4}[kind]]=mapped
                                image.write_bytes(seed)
                                baseline_process=run(drivers['baseline'],'append-step',*baseline_numbers,allowed=(0,77,78,79))
                                baseline_raw=image.read_bytes();baseline_before=save_raw(failure/'baseline-before-fsck.exfat',baseline_raw)
                                assert all(baseline_raw[at:at+n]==seed[at:at+n] for at,n in extents)
                                baseline_repair=subprocess.run([fsck,'-y',str(image)],capture_output=True,text=True)
                                baseline_allocation,baseline_log=allocation(drivers['baseline'])
                                baseline_after=save_raw(failure/'baseline-after-fsck.exfat',image.read_bytes())
                                (failure/'baseline-allocation.log').write_text(baseline_log)
                                (failure/'baseline-fsck.log').write_text(baseline_repair.stdout+baseline_repair.stderr)
                                (failure/'baseline-operation.log').write_text(baseline_process.stdout+baseline_process.stderr)
                                assert baseline_allocation==allocation_result,'New allocation-repair regression'
                                (failure/'comparison.json').write_text(json.dumps(dict(candidate=allocation_result,baseline=baseline_allocation,kind=kind,number=number,baseline_numbers=baseline_numbers,raw_sha256=dict(candidate_before=candidate_before,candidate_after=candidate_after,baseline_before=baseline_before,baseline_after=baseline_after),exfat_source_sha256=hashlib.sha256((ROOT/'kernel/src/fs/exfat.c').read_bytes()).hexdigest()),indent=2)+'\n')
                                image.write_bytes(repaired)
                            result['faults'].append({'inside_cluster':inside,'kind':kind,'number':number,'lost_name_after_offline_repair':lost,'local_complete':local_complete,'allocation':allocation_result,'baseline_allocation':baseline_allocation})
    # Exact native-size workload, actual shared process contents and foreign
    # global sync. This seed intentionally does not force allocator adjacency.
    f=empty_image(32*1024*1024,cluster_shift=8)
    f.add_directory_files('work',{'capture.tmp':b'','other.txt':b'','anchor.txt':b'unchanged'})
    growth_seed=bytes(f.data);growth={}
    for name,driver in drivers.items():
        image.write_bytes(growth_seed);process=run(driver,'sync-content-contiguous-growth')
        (out/f'growth-{name}.log').write_text(process.stdout+process.stderr)
        actual=ExfatFixture(image.read_bytes());expected=bytes((i*37+11)&255 for i in range(BYTES))
        assert actual.read('work/capture.bmp')==expected and actual.read('work/other.txt')==b'A'*17
        assert actual.read('work/anchor.txt')==b'unchanged';check_clean_image(image,fsck)
        rows=re.findall(r'contiguous-growth: round=(\d+) write_bytes=(\d+) size=(\d+)',process.stdout)
        assert len(rows)==65 and [int(r[0]) for r in rows]==list(range(1,66))
        assert sum(int(r[1]) for r in rows)==BYTES and max(int(r[1]) for r in rows)==65536
        reads=re.findall(r'contiguous-read: round=(\d+) role=3 offset=(\d+) bytes=(\d+) ok=1',process.stdout)
        assert sum(int(r[2]) for r in reads)==136314880,'Full committed-prefix reads changed'
        summary=re.search(r'sync-content-trace: callbacks=(\d+) hash=([0-9a-f]+) writes=(\d+) flushes=(\d+) write_bytes=(\d+)',process.stdout);assert summary
        growth[name]={'callbacks':int(summary[1]),'writes':int(summary[3]),'flushes':int(summary[4]),'write_bytes':int(summary[5]),'callback_hash':summary[2],'prefix_reads':len(reads),'prefix_read_bytes':sum(int(r[2]) for r in reads),'payload_sha256':hashlib.sha256(expected).hexdigest()}
        if name=='candidate':
            growth[name]['contiguous_counters']=append_contiguity([process.stdout.encode()])
            assert not growth[name]['contiguous_counters']['exercised'],'Ordinary growth adjacency changed; review actual counts'
    if 'baseline' in growth:
        assert growth['candidate']['flushes']==growth['baseline']['flushes']
        assert growth['candidate']['payload_sha256']==growth['baseline']['payload_sha256']
        assert growth['candidate']['callback_hash']==growth['baseline']['callback_hash']
    if not args.skip_faults:
        assert any(row['local_complete'] for row in result['faults'] if row['kind'] in ('write-fail','flush-fail')), 'Outer failure with local completion was not exercised'
    result['baseline_comparison']='historical_source' if args.baseline_exfat else 'same_source_host_off'
    result['known_incomplete_repairs']=sum(row['allocation']['status']=='FAIL' for row in result['faults'])
    dependencies=['kernel/src/fs/exfat.c','kernel/src/fs/exfat_stream.inc','kernel/src/process.c',
                  'kernel/src/sync_timing.c','kernel/src/vfs.c','kernel/src/vfs_namespace.c',
                  'kernel/src/autotest_storage.c','kernel/include/srvros/exfat.h',
                  'tools/exfat_host_driver.c','tools/test_process_writeback.c',
                  'tools/test_sync_content_bridge.inc','tools/test_contiguous_append.py',
                  'tools/test_contiguous_append_driver.c','tools/test_contiguous_append_process.c',
                  'tools/append_contiguity.py']
    result['source_sha256']={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in dependencies}
    result['baseline_exfat_sha256']=hashlib.sha256(baseline.read_bytes()).hexdigest()
    result['fault_scope']='Committed old payload/anchor preserved; all strict repaired-allocation failures are retained and matched against equivalent baseline cuts. PASS does not mean every interrupted image or offline repair is allocation-clean.'
    result['growth']=growth
    result['status']=('PASS_SMOKE_ONLY' if args.skip_faults else
                      'PASS_ORIGINAL_INVARIANTS_WITH_BASELINE_REPAIR_GAPS' if result['known_incomplete_repairs'] else 'PASS')
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('contiguous-append:',result['status'],'positive adjacent proof, unchanged barriers/negative paths, full-size shared global sync and',len(result['faults']),'fault cuts;',result['known_incomplete_repairs'],'baseline-known strict allocation FAIL results after offline repair')


if __name__=='__main__':
    main()
