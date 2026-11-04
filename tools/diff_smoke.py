#!/usr/bin/env python3
"""Two-boot diff qualification using pinned host GNU references.

Requires staged guest diff, test_diff_probe.elf, GNU diff 3.12 and GNU patch.
Guest outputs are compared byte-for-byte to the host, and each changed unified
report is applied by host GNU patch. The local patch check is a narrow subset.
GUI and native upstream suites are separate gates; selected upstream input
fixtures are exercised in the guest without claiming their scripts ran there.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import random
import shutil
import socket
import subprocess
import tempfile
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from thread_smoke import has_fatal_exception

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = None
GUEST = '/fat/diff-test/'


def sha(data): return hashlib.sha256(data).hexdigest()


def cases():
    long4=b'x'*4095+b'\n';long64=b'y'*65535+b'\n';capline=long64
    original=b''.join(f'line {i:02d}\n'.encode() for i in range(1,31))
    changed=original.replace(b'line 03\n',b'line 03 changed\n').replace(b'line 24\n',b'line 24 changed\n')
    return {
        'same':(b'one\ntwo\n',b'one\ntwo\n'),
        'empty':(b'',b'new\n'),
        'prefix':(b'one\ntwo\n',b'zero\none\ntwo\n'),
        'middle':(b'one\ntwo\nthree\n',b'one\nadded\nthree\n'),
        'suffix':(b'one\ntwo\nthree\n',b'one\ntwo\n'),
        'repeat':(b'a\nb\na\nb\nc\n',b'a\na\nb\nc\nb\n'),
        'multihunk':(original,changed),
        'nonewline':(b'one\ntwo',b'one\nthree'),
        'dash':(b'head\n--- removed\ntail\n',b'head\nreplacement\ntail\n'),
        'long4':(long4+b'end\n',long4+b'change\n'),
        'long64':(long64,long64[:-2]+b'z\n'),
        'linecap':(long64,long64),
        'linecap-noend':(b'y'*65536,b'y'*65536),
        'countcap':(b'x\n'*8192,b'x\n'*8191+b'z\n'),
        'bytecap':(capline*4,capline*3+capline[:-2]+b'z\n'),
        'upstream-large-subopt':((ROOT/'ports/upstream/diffutils/tests/large-subopt.in1').read_bytes(),
                                  (ROOT/'ports/upstream/diffutils/tests/large-subopt.in2').read_bytes()),
        'upstream-bug-64316':(b'c\nd\ne\nf\ng\na\nh\ni\nj\nk\nl\n',
                              b'c\nd\ne\nf\ng\nb\nh\ni\nj\nk\nl\n'),
    }


def prepare(gnu_diff):
    pairs=cases();files={}
    for name,(before,after) in pairs.items(): files[name+'.a']=before;files[name+'.b']=after
    files.update({'badnul':b'one\x00two\n','overline':b'x'*65536+b'\n',
                  'overcount':b'x\n'*8192+b'x','overbytes':(b'y'*65535+b'\n')*4+b'\n',
                  'work.a':b''.join(f'old-{i:05d} xxxxxxxx\n'.encode() for i in range(8192)),
                  'work.b':b''.join(f'new-{i:05d} xxxxxxxx\n'.encode() for i in range(8192)),
                  'local-target':pairs['middle'][0]})
    random_a=random.Random(0xD1FF);random_b=random.Random(0xD1FE)
    files['cpu.a']=b''.join(bytes([97+random_a.randrange(8)])+b'\n' for _ in range(8192))
    files['cpu.b']=b''.join(bytes([97+random_b.randrange(8)])+b'\n' for _ in range(8192))
    refs={};env=dict(os.environ,LC_ALL='C',LANG='C')
    with tempfile.TemporaryDirectory(prefix='diff-reference-') as directory:
        root=Path(directory)
        for name,(before,after) in pairs.items():
            a=root/(name+'.a');b=root/(name+'.b');a.write_bytes(before);b.write_bytes(after)
            for mode,opts in [('normal',[]),('quiet',['-q']),('unified',['-u','-L','before','-L','after'])]:
                result=subprocess.run([gnu_diff,*opts,'--',str(a),str(b)],capture_output=True,env=env)
                if result.returncode not in (0,1) or result.stderr: raise RuntimeError(f'host reference failed: {name}/{mode}: {result.stderr!r}')
                output=result.stdout.replace(str(a).encode(),(GUEST+name+'.a').encode()).replace(str(b).encode(),(GUEST+name+'.b').encode())
                refs[(name,mode)]=(result.returncode,output)
    return files,refs


def workflow(pairs):
    lines=['#!/fat/bin/sh','chmod 755 /fat/diff-test/diff-probe','chmod 755 /fat/diff-test/diff-diagnostic']
    for name in pairs:
        a=GUEST+name+'.a';b=GUEST+name+'.b'
        for mode,opts in [('normal',''),('quiet','-q'),('unified','-u -L before -L after')]:
            lines.append(f'diff {opts} -- {a} {b} > {GUEST}{name}.{mode}.out 2> {GUEST}{name}.{mode}.err')
            lines.append(f'echo $? > {GUEST}{name}.{mode}.status')
    for name,args in [('nul',f'{GUEST}badnul {GUEST}same.a'),('line',f'{GUEST}overline {GUEST}same.a'),
                      ('count',f'{GUEST}overcount {GUEST}same.a'),('bytes',f'{GUEST}overbytes {GUEST}same.a'),
                      ('missing',f'{GUEST}missing {GUEST}same.a'),('directory',f'/fat/diff-test {GUEST}same.a'),
                      ('stdin',f'- {GUEST}same.a'),('unsupported',f'-U0 {GUEST}same.a {GUEST}same.b'),
                      ('nul-same',f'-- {GUEST}badnul {GUEST}badnul'),('nul-quiet',f'-q -- {GUEST}badnul {GUEST}badnul'),
                      ('line-quiet',f'-q -- {GUEST}overline {GUEST}overline'),
                      ('count-quiet',f'-q -- {GUEST}overcount {GUEST}overcount'),
                      ('bytes-quiet',f'-q -- {GUEST}overbytes {GUEST}overbytes')]:
        lines.append(f'diff {args} > {GUEST}fail-{name}.out 2> {GUEST}fail-{name}.err')
        lines.append(f'echo $? > {GUEST}fail-{name}.status')
    lines.append(f'{GUEST}diff-probe --stat > {GUEST}unlabeled.stat 2> {GUEST}unlabeled-stat.err')
    lines.append(f'echo $? > {GUEST}unlabeled-stat.status')
    lines.append(f'TZ=UTC LC_ALL=C diff -u -- {GUEST}middle.a {GUEST}middle.b > {GUEST}unlabeled.out 2> {GUEST}unlabeled.err')
    lines.append(f'echo $? > {GUEST}unlabeled.status')
    # Diagnostic-only link wrappers inject failure from the chosen allocation
    # onward. Each run is a fresh process and its partial report is retained.
    for index in (0,1,2,4,8,16,32):
        lines.append(f'SRV_DIFF_FAIL_AFTER={index} {GUEST}diff-diagnostic -u -L before -L after -- {GUEST}multihunk.a {GUEST}multihunk.b > {GUEST}alloc-{index}.out 2> {GUEST}alloc-{index}.err')
        lines.append(f'echo $? > {GUEST}alloc-{index}.status')
    lines.append(f'{GUEST}diff-diagnostic -u -L before -L after -- {GUEST}multihunk.a {GUEST}multihunk.b > {GUEST}alloc-recovery.out 2> {GUEST}alloc-recovery.err')
    lines.append(f'echo $? > {GUEST}alloc-recovery.status')
    lines += [f'patch -i {GUEST}middle.unified.out {GUEST}local-target > {GUEST}patch.log 2> {GUEST}patch.err',
              f'echo $? > {GUEST}patch.status',
              f'{GUEST}diff-probe > {GUEST}probe.log 2> {GUEST}probe.err',
              f'echo $? > {GUEST}probe.status','echo DIFF_WORKFLOW_DONE']
    return ('\n'.join(lines)+'\n').encode()


def main():
    global EVIDENCE
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--qemu',default=os.environ.get('QEMU','qemu-system-x86_64'))
    ap.add_argument('--disk',type=Path,default=ROOT/'build/srvros.exfat')
    ap.add_argument('--probe',type=Path,default=ROOT/'build/userspace/test_diff_probe.elf')
    ap.add_argument('--diagnostic',type=Path,default=ROOT/'build/userspace/diff-diagnostic.elf')
    ap.add_argument('--gnu-diff',required=True);ap.add_argument('--gnu-patch',required=True)
    args=ap.parse_args();env=dict(os.environ,LC_ALL='C',LANG='C')
    diff_version=subprocess.check_output([args.gnu_diff,'--version'],text=True,env=env).splitlines()[0]
    patch_version=subprocess.check_output([args.gnu_patch,'--version'],text=True,env=env).splitlines()[0]
    if not re.search(r'\bGNU diffutils\)? 3\.12\b',diff_version): raise RuntimeError('GNU diffutils 3.12 required: '+diff_version)
    if 'GNU patch' not in patch_version: raise RuntimeError('GNU patch required: '+patch_version)
    base=ROOT/'build/review/diff';base.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'),dir=base));EVIDENCE=out
    disk=out/'disk.exfat';shutil.copyfile(args.disk,disk);img=ExfatFixture(disk.read_bytes())
    binary=(ROOT/'build/userspace/diff.elf').read_bytes();staged=img.read('bin/diff')
    if staged!=runtime_elf(binary):raise RuntimeError('staged diff differs from build')
    files,refs=prepare(args.gnu_diff);files['workflow.sh']=workflow(cases());files['diff-probe']=runtime_elf(args.probe.read_bytes());files['diff-diagnostic']=runtime_elf(args.diagnostic.read_bytes())
    img.add_directory_files('diff-test',files);disk.write_bytes(img.data)
    source_files=[p for p in sorted((ROOT/'ports/srvros/diffutils').rglob('*')) if p.is_file()]
    receipt={'status':'INCOMPLETE','source_disk_sha256':sha(args.disk.read_bytes()),'diff_sha256':sha(binary),
             'staged_diff_sha256':sha(staged),'probe_sha256':sha(files['diff-probe']),'diagnostic_sha256':sha(files['diff-diagnostic']),
             'fixtures_sha256':{k:sha(v) for k,v in files.items()},'host_diff':diff_version,'host_patch':patch_version,
             'reference_sha256':{f'{n}.{m}':sha(v[1]) for (n,m),v in refs.items()},
             'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
             'source_status':subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True),
             'source_diff_sha256':sha(subprocess.check_output(['git','diff','--binary','HEAD'],cwd=ROOT)),
             'port_source_sha256':{str(p.relative_to(ROOT)):sha(p.read_bytes()) for p in source_files},
             'kernel_sha256':sha((ROOT/'build/iso_root/boot/srvros.elf').read_bytes()),
             'initramfs_sha256':sha((ROOT/'build/iso_root/boot/initramfs.tar').read_bytes()),
             'upstream_guest_fixtures':['large-subopt.in1/in2 unchanged','bug-64316 generated input pair'],
             'scope':'comparison, host patch and local patch subset, injected allocation failure, subprocess signals after algorithm progress; selected upstream inputs in guest, not upstream test scripts; no in-call timing claim; GUI and native upstream suite separate'}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    print('diff evidence:',out,flush=True)
    with tempfile.TemporaryDirectory(prefix='diff-vm-') as temp:
        temp=Path(temp);iso=build_iso(str(ROOT),str(temp),1280,800,os.environ.copy())
        for boot in (1,2):
            output=bytearray();consumed=0;port=choose_port(30001,38000);qmp_path=temp/f'qmp{boot}'
            cmd=[args.qemu,'-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
                 '-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci',
                 '-device','ide-hd,drive=disk,bus=ahci.0','-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
                 '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-qmp',f'unix:{qmp_path},server=on,wait=off',
                 '-display','none','-monitor','none','-no-reboot']
            serial=qmp=None
            with (out/f'qemu-{boot}.log').open('wb') as log:
                vm=subprocess.Popen(cmd,stdout=log,stderr=log)
                try:
                    serial=connect_serial(port,20);serial.settimeout(.2);qmp=QMP(qmp_path)
                    def wait(marker,timeout):
                        nonlocal consumed
                        start=time.monotonic()
                        while marker not in output[consumed:]:
                            if marker==b'srv> ' and not consumed:qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                            if time.monotonic()-start>timeout or vm.poll() is not None:raise RuntimeError(f'boot {boot}: missing {marker!r}')
                            try:
                                chunk=serial.recv(16384)
                                if not chunk:raise RuntimeError('serial disconnected')
                                output.extend(chunk)
                            except socket.timeout:pass
                        consumed=output.index(marker,consumed)+len(marker)
                    wait(b'srv> ',100)
                    if boot==1:
                        serial.sendall(b'run /fat/bin/sh /fat/diff-test/workflow.sh\n');wait(b'DIFF_WORKFLOW_DONE',300);wait(b'srv> ',40)
                    else:
                        serial.sendall(b'run /fat/bin/sh\n');wait(b' $ ',40)
                        serial.sendall(b'diff -u -L before -L after -- /fat/diff-test/middle.a /fat/diff-test/middle.b > /fat/diff-test/reboot.out\n');wait(b' $ ',60)
                        serial.sendall(b'echo $? > /fat/diff-test/reboot.status\n');wait(b' $ ',40)
                        serial.sendall(b'exit\n');wait(b'srv> ',40)
                    serial.sendall(b'fsck /fat\n');wait(b'exfat-check:',40);wait(b'srv> ',40)
                    if not re.search(rb'exfat-check: mounts=1 files=\d+ clusters=\d+ leaks=0 fat=0 errors=0 ok',output):raise RuntimeError('guest fsck failed')
                    if has_fatal_exception(output.decode(errors='replace')):raise RuntimeError('guest fatal exception')
                finally:
                    if serial:serial.close()
                    if qmp:qmp.stream.close();qmp.sock.close()
                    stop_process(vm);(out/f'serial-{boot}.log').write_bytes(output)
            img=ExfatFixture(disk.read_bytes())
            for name,data in files.items():
                if name!='local-target' and img.read('diff-test/'+name)!=data:raise RuntimeError('fixture modified: '+name)
    img=ExfatFixture(disk.read_bytes())
    def get(name):
        data=img.read('diff-test/'+name);(out/name).write_bytes(data);return data
    # Retain all available diagnostics before checking any status.
    for name in [f'{n}.{m}.{suffix}' for n in cases() for m in ('normal','quiet','unified') for suffix in ('out','err','status')]+[f'fail-{n}.{suffix}' for n in ('nul','line','count','bytes','missing','directory','stdin','unsupported','nul-same','nul-quiet','line-quiet','count-quiet','bytes-quiet') for suffix in ('out','err','status')]+['probe.log','probe.err','probe.status','patch.log','patch.err','patch.status','reboot.out','reboot.status','local-target','cpu-partial.out',
      'unlabeled.stat','unlabeled-stat.err','unlabeled-stat.status','unlabeled.out','unlabeled.err','unlabeled.status']:
        try:get(name)
        except StopIteration:pass
    for index in (0,1,2,4,8,16,32):
        for suffix in ('out','err','status'):
            try:get(f'alloc-{index}.{suffix}')
            except StopIteration:pass
    for suffix in ('out','err','status'):
        try:get(f'alloc-recovery.{suffix}')
        except StopIteration:pass
    hit_indices=[];hit_families=set()
    for index in (0,1,2,4,8,16,32):
        status=get(f'alloc-{index}.status');err=get(f'alloc-{index}.err')
        hit=b'DIFF_ALLOC_FAIL' in err
        if hit:
            match=re.search(rb'DIFF_ALLOC_FAIL (malloc|calloc|realloc)\b',err)
            if not match:raise RuntimeError(f'allocation failure {index} has invalid marker: {err!r}')
            hit_families.add(match.group(1).decode())
            hit_indices.append(index)
            if status!=b'2\n':raise RuntimeError(f'injected allocation failure {index} returned {status!r}')
        elif index in (0,1,2,4):
            raise RuntimeError(f'allocation failure {index} was not observed')
        elif status!=b'1\n' or get(f'alloc-{index}.out')!=refs[('multihunk','unified')][1]:
            raise RuntimeError(f'allocation test {index}: no hook hit and invalid comparison')
    if (get('alloc-recovery.status')!=b'1\n' or get('alloc-recovery.err') or
        get('alloc-recovery.out')!=refs[('multihunk','unified')][1]):
        raise RuntimeError('diagnostic comparison did not recover after allocation tests')
    receipt['allocation_failure_hit_indices']=hit_indices
    receipt['allocation_failure_families_observed']=sorted(hit_families)
    receipt['gate_results']={'injected_allocation_failure_and_recovery':'PASS'}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    for (name,mode),(status,expected) in refs.items():
        if get(f'{name}.{mode}.status')!=f'{status}\n'.encode():raise RuntimeError(f'{name}/{mode}: status differs')
        if get(f'{name}.{mode}.out')!=expected:raise RuntimeError(f'{name}/{mode}: output differs from GNU 3.12')
        if get(f'{name}.{mode}.err'):raise RuntimeError(f'{name}/{mode}: unexpected stderr')
    for name in ('nul','line','count','bytes','missing','directory','stdin','unsupported','nul-same','nul-quiet','line-quiet','count-quiet','bytes-quiet'):
        if get(f'fail-{name}.status')!=b'2\n' or not get(f'fail-{name}.err'):
            raise RuntimeError(f'{name}: expected status 2 and diagnostic')
    if get('unlabeled-stat.status')!=b'0\n' or get('unlabeled-stat.err') or get('unlabeled.status')!=b'1\n' or get('unlabeled.err'):
        raise RuntimeError('unlabeled timestamp collection or diff failed')
    stat_lines=get('unlabeled.stat').decode('ascii').splitlines()
    if len(stat_lines)!=2:raise RuntimeError('unexpected guest timestamp output')
    timestamps=[]
    for line,name in zip(stat_lines,('middle.a','middle.b')):
        parts=line.split()
        if len(parts)!=3 or parts[0]!=GUEST+name:raise RuntimeError('invalid guest timestamp: '+line)
        sec,nsec=int(parts[1]),int(parts[2])
        if not 0<=nsec<1000000000:raise RuntimeError('invalid guest nanoseconds')
        timestamps.append(sec*1000000000+nsec)
    with tempfile.TemporaryDirectory(prefix='diff-timestamp-reference-') as directory:
        root=Path(directory);paths=[root/'middle.a',root/'middle.b']
        for path,name,ns in zip(paths,('middle.a','middle.b'),timestamps):
            path.write_bytes(files[name]);os.utime(path,ns=(ns,ns))
            if path.stat().st_mtime_ns!=ns:raise RuntimeError('host filesystem cannot represent guest timestamp exactly')
        refenv=dict(env,TZ='UTC')
        result=subprocess.run([args.gnu_diff,'-u','--',str(paths[0]),str(paths[1])],capture_output=True,env=refenv)
        if result.returncode!=1 or result.stderr:raise RuntimeError('host unlabeled reference failed')
        expected=result.stdout.replace(str(paths[0]).encode(),(GUEST+'middle.a').encode()).replace(str(paths[1]).encode(),(GUEST+'middle.b').encode())
        (out/'unlabeled-reference.out').write_bytes(expected)
        if get('unlabeled.out')!=expected:raise RuntimeError('unlabeled unified report differs from timestamp-matched host reference')
        receipt['unlabeled_reference_sha256']=sha(expected)
        receipt['unlabeled_guest_mtime_ns']=timestamps
    if get('patch.status')!=b'0\n' or get('local-target')!=files['middle.b']:raise RuntimeError('local patch subset failed')
    if get('reboot.status')!=b'1\n' or get('reboot.out')!=refs[('middle','unified')][1]:raise RuntimeError('reboot report mismatch')
    receipt['gate_results'].update(comparison_reference='PASS', input_refusals='PASS', unlabeled_timestamp_reference='PASS', local_patch_subset='PASS', second_boot_report='PASS')
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    if get('probe.status')!=b'0\n' or b'diff-probe: PASS' not in get('probe.log'):raise RuntimeError('subprocess probe failed')
    with tempfile.TemporaryDirectory(prefix='diff-patch-reference-') as directory:
        root=Path(directory)
        for name,(before,after) in cases().items():
            if before==after:continue
            src=root/(name+'.src');dst=root/(name+'.dst');src.write_bytes(before)
            result=subprocess.run([args.gnu_patch,'--batch','--silent','-o',str(dst),str(src)],
                                  input=get(name+'.unified.out'),capture_output=True,env=env)
            if result.returncode or dst.read_bytes()!=after:raise RuntimeError(f'GNU patch failed {name}: {result.stderr!r}')
    receipt['gate_results'].update(subprocess_signals_and_output_errors='PASS',host_gnu_patch_roundtrips='PASS')
    receipt['status']='PASS';(out/'receipt.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    print('diff smoke: PASS guest/reference and host patch; GUI and upstream suite separate:',out,flush=True)


if __name__=='__main__':
    try:main()
    except Exception as error:
        if EVIDENCE:
            path=EVIDENCE/'receipt.json';receipt=json.loads(path.read_text()) if path.exists() else {}
            receipt.update(status='FAIL',error=f'{type(error).__name__}: {error}');path.write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
        raise
