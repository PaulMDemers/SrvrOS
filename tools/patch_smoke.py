#!/usr/bin/env python3
"""Two-boot bounded patch qualification on a disposable image.

Host references must be GNU diff 3.12 and GNU patch 2.8.1. This harness is
The receipt reports only this harness; GUI, reproducible builds and upstream
suites are separate gates. It does not claim general GNU patch compatibility.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
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

ROOT=Path(__file__).resolve().parents[1]
GUEST='/fat/patch-test/'
EVIDENCE=None


def sha(data):return hashlib.sha256(data).hexdigest()


def pairs():
    text=b''.join(f'line-{i:02d}\n'.encode() for i in range(1,31))
    longline=b'L'*65535+b'\n'
    envelope=longline+(b'x'*24+b'\n')*24+(b'x'*23+b'\n')*(8191-24)
    return {
        'replace':(b'alpha\nbeta\ngamma\n',b'alpha\nnew\ngamma\n',False),
        'empty-in':(b'',b'created content\n',False),
        'empty-out':(b'removed content\n',b'',False),
        'long64-noend':(b'U'*65536,b'U'*65535+b'V',False),
        'insert0':(b'alpha\nbeta\n',b'alpha\nnew\nbeta\n',True),
        'delete0':(b'alpha\nnew\nbeta\n',b'alpha\nbeta\n',True),
        'dash':(b'one\n--- removed\nthree\n',b'one\nreplacement\nthree\n',False),
        'headerlike':(b'one\n-- removed\nthree\n',b'one\nreplacement\nthree\n',False),
        'multihunk':(text,text.replace(b'line-03\n',b'change-03\n').replace(b'line-24\n',b'change-24\n'),False),
        'nonewline':(b'one\ntwo',b'one\nthree',False),
        'long4':(b'x'*4095+b'\nold\n',b'x'*4095+b'\nnew\n',False),
        'long64':(longline,longline[:-2]+b'R\n',False),
        'envelope':(envelope,envelope[:-2]+b'z\n',False),
    }


def make_patch(diff,base,src,dst,zero=False,labels=('before','after')):
    a=base/'input-a';b=base/'input-b';a.write_bytes(src);b.write_bytes(dst)
    cmd=[diff,'-U0' if zero else '-u','-L',labels[0],'-L',labels[1],'--',str(a),str(b)]
    result=subprocess.run(cmd,capture_output=True,env=dict(os.environ,LC_ALL='C',TZ='UTC'))
    if result.returncode!=1 or result.stderr:raise RuntimeError(f'host diff failed {result.returncode}: {result.stderr!r}')
    return result.stdout


def fixtures(host_diff,host_patch):
    files={};expected={}
    with tempfile.TemporaryDirectory(prefix='patch-reference-') as td:
        base=Path(td)
        for name,(before,after,zero) in pairs().items():
            patch=make_patch(host_diff,base,before,after,zero,('a/name','b/name') if name=='dash' else ('before','after'))
            src=base/'source';out=base/'result';report=base/'change';src.write_bytes(before);report.write_bytes(patch)
            if out.exists():out.unlink()
            result=subprocess.run([host_patch,'--batch','--forward','--fuzz=0','-i',str(report),'-o',str(out),str(src)],capture_output=True,env=dict(os.environ,LC_ALL='C',TZ='UTC'))
            if result.returncode or not out.exists() or out.read_bytes()!=after:raise RuntimeError(f'host patch failed {name}: {result.stderr!r}')
            files[name+'.src']=before;files[name+'.patch']=patch;expected[name]=after
        files['malformed.patch']=b'--- before\n+++ after\n@@ bad @@\n-old\n+new\n'
        files['truncated.patch']=b'--- before\n+++ after\n@@ -1,2 +1,2 @@\n old\n'
        files['overflow.patch']=b'--- before\n+++ after\n@@ -999999999999999999999999,1 +1,1 @@\n-old\n+new\n'
        files['multi.patch']=files['replace.patch']+files['replace.patch']
        files['unsafe.patch']=files['replace.patch'].replace(b'--- before\n',b'--- ../../escape\n',1)
        files['--version']=files['replace.src']
        files['race-source.src']=files['replace.src'];files['race-source.patch']=files['replace.patch']
        files['race-patch.src']=files['replace.src'];files['race-patch.patch']=files['replace.patch']
        files['race-grow.src']=files['replace.src'];files['race-grow.patch']=files['replace.patch']
        files['unmatched.src']=b'not alpha\nnot beta\nnot gamma\n'
        files['existing.out']=b'existing destination must survive\n'
        files['over-source']=b'x\n'*8193
        files['over-source-bytes']=(b'x'*65535+b'\n')*4+b'x'
        files['over-source-line']=b'x'*65536+b'\n'
        files['nul-source']=b'alpha\x00beta\n'
        files['nul-patch']=b'--- before\n+++ after\n@@ -1 +1 @@\n-a\x00b\n+c\n'
        files['over-patch']=b'X'*(1024*1024+1)
        files['over-patch-lines']=b'--- before\n+++ after\n'+b' x\n'*32767
        files['over-patch-line']=b'--- before\n+++ after\n@@ -1 +1 @@\n-'+b'x'*131072+b'\n'
        files['output-lines.src']=b'x\n'*8192
        files['over-output-lines.patch']=make_patch(host_diff,base,files['output-lines.src'],files['output-lines.src']+b'z\n')
        files['over-output-line.patch']=make_patch(host_diff,base,files['long64.src'],b'L'*65536+b'\n')
        before=files['envelope.src'];after=before[:65536]+b'x'+before[65536:]
        files['over-output.patch']=make_patch(host_diff,base,before,after)
        # A valid hunk whose declared starting line is far from its actual
        # unique context forces offset searches before the diagnostic marker.
        cpu=b''.join(f'item-{i:05d}\n'.encode() for i in range(8192))
        changed=cpu.replace(b'item-07000\n',b'edit-07000\n',1)
        report=make_patch(host_diff,base,cpu,changed)
        report,count=re.subn(rb'@@ -[0-9]+,([0-9]+) \+[0-9]+,([0-9]+) @@',rb'@@ -1,\1 +1,\2 @@',report,count=1)
        if count!=1:raise RuntimeError('could not construct offset-search fixture')
        files['cpu.src']=cpu;files['cpu.patch']=report
    return files,expected


def workflow():
    lines=['#!/fat/bin/sh','chmod 755 /fat/patch-test/patch-probe','chmod 755 /fat/patch-test/patch-diagnostic']
    for name in pairs():
        lines.append(f'patch -i {GUEST}{name}.patch -o {GUEST}{name}.out {GUEST}{name}.src > {GUEST}{name}.log 2> {GUEST}{name}.err')
        lines.append(f'echo $? > {GUEST}{name}.status')
    # The source really is a relative option-looking name, not an absolute
    # path with a harmless dash later in the string.
    lines += [f'cd {GUEST}',
              f'patch -i {GUEST}replace.patch -o {GUEST}dash-operand.out --version > {GUEST}dash-operand.log 2> {GUEST}dash-operand.err',
              f'echo $? > {GUEST}dash-operand.status']
    failures=[('malformed','malformed.patch','replace.src','malformed.out'),
              ('truncated','truncated.patch','replace.src','truncated.out'),
              ('overflow','overflow.patch','replace.src','overflow.out'),
              ('multi','multi.patch','replace.src','multi.out'),
              ('unsafe','unsafe.patch','replace.src','unsafe.out'),
              ('failed-hunk','replace.patch','unmatched.src','failed-hunk.out'),
              ('existing','replace.patch','replace.src','existing.out'),
              ('alias','replace.patch','replace.src','./replace.src'),
              ('over-source','replace.patch','over-source','over-source.out'),
              ('source-bytes','replace.patch','over-source-bytes','source-bytes.out'),
              ('source-line','replace.patch','over-source-line','source-line.out'),
              ('source-nul','replace.patch','nul-source','source-nul.out'),
              ('patch-nul','nul-patch','replace.src','patch-nul.out'),
              ('patch-records','over-patch-lines','replace.src','patch-records.out'),
              ('patch-line','over-patch-line','replace.src','patch-line.out'),
              ('missing-source','replace.patch','no-such-source','missing-source.out'),
              ('missing-patch','no-such-patch','replace.src','missing-patch.out'),
              ('over-patch','over-patch','replace.src','over-patch.out'),
              ('over-output','over-output.patch','envelope.src','over-output.out'),
              ('output-lines','over-output-lines.patch','output-lines.src','output-lines.out'),
              ('output-line','over-output-line.patch','long64.src','output-line.out')]
    for name,patch,source,target in failures:
        lines.append(f'patch -i {GUEST}{patch} -o {GUEST}{target} {GUEST}{source} > {GUEST}fail-{name}.log 2> {GUEST}fail-{name}.err')
        lines.append(f'echo $? > {GUEST}fail-{name}.status')
    for index in (0,1,2,4,8,16,32):
        lines.append(f'SRV_PATCH_FAIL_AFTER={index} {GUEST}patch-diagnostic -i {GUEST}replace.patch -o {GUEST}alloc-{index}.out {GUEST}replace.src > {GUEST}alloc-{index}.log 2> {GUEST}alloc-{index}.err')
        lines.append(f'echo $? > {GUEST}alloc-{index}.status')
    lines.append(f'{GUEST}patch-diagnostic -i {GUEST}replace.patch -o {GUEST}alloc-recovery.out {GUEST}replace.src > {GUEST}alloc-recovery.log 2> {GUEST}alloc-recovery.err')
    lines.append(f'echo $? > {GUEST}alloc-recovery.status')
    lines += [f'{GUEST}patch-probe > {GUEST}probe.log 2> {GUEST}probe.err',
              f'echo $? > {GUEST}probe.status','echo PATCH_WORKFLOW_DONE']
    return ('\n'.join(lines)+'\n').encode()


def entries(image,directory):
    return {name for name,_ in image.entries(image.read(directory))}


def validate_metadata(data):
    """Validate every serialized exFAT metadata row, not just its magic.

    Matches the record shape checked by profile_chooser_smoke.metadata_rows,
    with the kernel's ASCII pathname and uint64 bounds applied as well.
    """
    if not data.startswith(b'SRVROSMETA1\n') or not data.endswith(b'\n'):
        raise RuntimeError('invalid metadata sidecar header or final newline')
    try:
        lines=data.decode('ascii').split('\n')
    except UnicodeDecodeError as exc:
        raise RuntimeError('invalid metadata sidecar encoding') from exc
    seen=set()
    for line in lines[1:-1]:
        columns=line.split('\t')
        if len(columns)!=9:
            raise RuntimeError('invalid metadata sidecar record')
        path=columns[0]
        if (len(path)>1023 or any(ord(c)<32 or ord(c)>126 for c in path) or
            not (path=='/fat' or path.startswith('/fat/')) or
            path=='/fat/.srvros' or path.startswith('/fat/.srvros/') or
            (path!='/fat' and any(part in ('','.','..') for part in path[5:].split('/'))) or path in seen):
            raise RuntimeError('invalid or duplicate metadata sidecar path')
        seen.add(path)
        for value in columns[1:]:
            digits=value.lstrip('0')
            if (not re.fullmatch(r'[0-9]+',value) or len(digits)>20 or
                (digits and int(digits)>2**64-1)):
                raise RuntimeError('invalid metadata sidecar uint64 field')
    return len(seen)


def directory_snapshot(image):
    root={name for name,_ in image.entries(image.chain(image.root))}
    sidecar={}
    if '.srvros' in root:
        for name,entry in image.entries(image.read('.srvros')):
            data=image.read('.srvros/'+name)
            sidecar[name]={'sha256':sha(data),'size':len(data),
                           'attributes':int.from_bytes(entry[4:6],'little')}
    return {'root':sorted(root),'home':sorted(entries(image,'home')),
            'tmp':sorted(entries(image,'tmp')),'sidecar':sidecar}


def main():
    global EVIDENCE
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--qemu',default=os.environ.get('QEMU','qemu-system-x86_64'))
    ap.add_argument('--disk',type=Path,default=ROOT/'build/srvros.exfat')
    ap.add_argument('--probe',type=Path,default=ROOT/'build/userspace/test_patch_probe.elf')
    ap.add_argument('--diagnostic',type=Path,default=ROOT/'build/userspace/patch-diagnostic.elf')
    ap.add_argument('--gnu-diff',required=True);ap.add_argument('--gnu-patch',required=True)
    args=ap.parse_args();env=dict(os.environ,LC_ALL='C',TZ='UTC')
    dv=subprocess.check_output([args.gnu_diff,'--version'],text=True,env=env).splitlines()[0]
    pv=subprocess.check_output([args.gnu_patch,'--version'],text=True,env=env).splitlines()[0]
    if not re.search(r'\bGNU diffutils\)? 3\.12\b',dv):raise RuntimeError('GNU diff 3.12 required: '+dv)
    if not re.search(r'GNU patch 2\.8\.1\b',pv):raise RuntimeError('GNU patch 2.8.1 required: '+pv)
    base=ROOT/'build/review/patch';base.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'),dir=base));EVIDENCE=out
    disk=out/'disk.exfat';shutil.copyfile(args.disk,disk);image=ExfatFixture(disk.read_bytes())
    binary=(ROOT/'build/userspace/patch.elf').read_bytes();staged=image.read('bin/patch')
    if staged!=runtime_elf(binary):raise RuntimeError('staged patch differs from build')
    files,expected=fixtures(args.gnu_diff,args.gnu_patch);files['multihunk.expected']=expected['multihunk']
    files['workflow.sh']=workflow();files['reboot.sh']=(b'#!/fat/bin/sh\ncmp -s /fat/patch-test/multihunk.out /fat/patch-test/multihunk.expected\necho $? > /fat/patch-test/reboot.status\necho PATCH_REBOOT_DONE\n')
    files['patch-probe']=runtime_elf(args.probe.read_bytes());files['patch-diagnostic']=runtime_elf(args.diagnostic.read_bytes())
    baseline=directory_snapshot(image);root_before=set(baseline['root'])
    if 'meta.tmp' in baseline['sidecar']:raise RuntimeError('source image contains an unfinished metadata sidecar')
    if 'patch-test' in root_before:raise RuntimeError('source image already contains patch-test')
    directory_evidence={'before':baseline}
    (out/'directory-evidence.json').write_text(json.dumps(directory_evidence,indent=2,sort_keys=True)+'\n')
    image.add_directory_files('patch-test',files);disk.write_bytes(image.data)
    source_files=[p for p in sorted((ROOT/'ports/srvros/patch').rglob('*')) if p.is_file()]
    receipt={'status':'INCOMPLETE','source_disk_sha256':sha(args.disk.read_bytes()),'patch_sha256':sha(binary),
             'kernel_sha256':sha((ROOT/'build/iso_root/boot/srvros.elf').read_bytes()),
             'initramfs_sha256':sha((ROOT/'build/iso_root/boot/initramfs.tar').read_bytes()),
             'port_source_sha256':{str(p.relative_to(ROOT)):sha(p.read_bytes()) for p in source_files},
             'harness_source_sha256':{str(p.relative_to(ROOT)):sha(p.read_bytes()) for p in (ROOT/'tools/patch_smoke.py',ROOT/'tools/test_patch_probe.c')},
             'host_reference_binary_sha256':{'diff':sha(Path(args.gnu_diff).read_bytes()),'patch':sha(Path(args.gnu_patch).read_bytes())},
             'runtime_patch_sha256':sha(staged),'probe_sha256':sha(files['patch-probe']),'diagnostic_sha256':sha(files['patch-diagnostic']),
             'fixtures_sha256':{k:sha(v) for k,v in files.items()},'expected_sha256':{k:sha(v) for k,v in expected.items()},
             'host_diff':dv,'host_patch':pv,'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
             'source_status':subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True),
             'source_diff_sha256':sha(subprocess.check_output(['git','diff','--binary','HEAD'],cwd=ROOT)),
             'scope':'this two-boot guest harness only; diagnostic signals are after progress, not in-syscall; GUI, native upstream tests and reproducible builds separate',
             'gate_results':{},'pending':['this guest harness','GUI Terminal','native upstream tests','two clean builds']}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    print('patch evidence:',out,flush=True)
    with tempfile.TemporaryDirectory(prefix='patch-vm-') as temp:
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
                        serial.sendall(b'run /fat/bin/sh /fat/patch-test/workflow.sh\n');wait(b'PATCH_WORKFLOW_DONE',300);wait(b'srv> ',40)
                    else:
                        serial.sendall(b'run /fat/bin/sh /fat/patch-test/reboot.sh\n')
                        wait(b'PATCH_REBOOT_DONE',60);wait(b'srv> ',40)
                    serial.sendall(b'fsck /fat\n');wait(b'exfat-check:',40);wait(b'srv> ',40)
                    if not re.search(rb'exfat-check: mounts=1 files=\d+ clusters=\d+ leaks=0 fat=0 errors=0 ok',output):raise RuntimeError('guest fsck failed')
                    if has_fatal_exception(output.decode(errors='replace')):raise RuntimeError('guest fatal exception')
                finally:
                    if serial:serial.close()
                    if qmp:qmp.stream.close();qmp.sock.close()
                    stop_process(vm);(out/f'serial-{boot}.log').write_bytes(output)
            image=ExfatFixture(disk.read_bytes())
            directory_evidence[f'boot-{boot}']=directory_snapshot(image)
            (out/'directory-evidence.json').write_text(json.dumps(directory_evidence,indent=2,sort_keys=True)+'\n')
            # Retain guest files before checking preservation or later gates so
            # failed comparisons leave directly reviewable evidence as well as
            # the full disposable image.
            captured=out/f'guest-boot-{boot}';captured.mkdir()
            for name in entries(image,'patch-test'):
                if name not in ('.','..'):
                    (captured/name).write_bytes(image.read('patch-test/'+name))
            for name,data in files.items():
                if name in ('race-source.src','race-patch.patch'):
                    if name in entries(image,'patch-test'):raise RuntimeError('race input was not removed: '+name)
                elif name=='race-grow.src':
                    if image.read('patch-test/'+name)!=b'G'*(256*1024+1):raise RuntimeError('race input growth differs')
                elif image.read('patch-test/'+name)!=data:raise RuntimeError('fixture modified: '+name)
    image=ExfatFixture(disk.read_bytes())
    final=directory_evidence['boot-2'];root_after=set(final['root'])
    before_sidecar=baseline['sidecar'];after_sidecar=final['sidecar']
    if (root_after!=root_before|{'patch-test','.srvros'} or final['home']!=baseline['home'] or
        final['tmp']!=baseline['tmp'] or 'meta.tmp' in after_sidecar or
        set(after_sidecar)!=set(before_sidecar)|{'meta'}):
        raise RuntimeError('unexpected artifact outside fixture directory; see directory-evidence.json')
    for name,record in before_sidecar.items():
        if name!='meta' and after_sidecar.get(name)!=record:
            raise RuntimeError('pre-existing sidecar artifact changed: '+name)
    if after_sidecar['meta']['attributes'] & 0x10:
        raise RuntimeError('metadata sidecar is not a regular file')
    receipt['metadata_sidecar_records']=validate_metadata(image.read('.srvros/meta'))
    receipt['directory_evidence_sha256']=sha((out/'directory-evidence.json').read_bytes())
    def get(name):
        data=image.read('patch-test/'+name);(out/name).write_bytes(data);return data
    failures=('malformed','truncated','overflow','multi','unsafe','failed-hunk','existing','alias',
              'over-source','source-bytes','source-line','source-nul','patch-nul','patch-records','patch-line',
              'missing-source','missing-patch','over-patch','over-output','output-lines','output-line')
    names=[f'{n}.{s}' for n in pairs() for s in ('out','log','err','status')]
    names += [f'fail-{n}.{s}' for n in failures for s in ('log','err','status')]
    names += ['probe.log','probe.err','probe.status','reboot.status','race-source.out','race-patch.out','race-grow.out',
              'dash-operand.out','dash-operand.log','dash-operand.err','dash-operand.status']
    names += [f'alloc-{i}.{s}' for i in (0,1,2,4,8,16,32) for s in ('log','err','status')]
    names += [f'alloc-recovery.{s}' for s in ('out','log','err','status')]
    for name in names:
        try:get(name)
        except StopIteration:pass
    for name,data in expected.items():
        if get(name+'.status')!=b'0\n' or get(name+'.out')!=data:raise RuntimeError('positive fixture failed: '+name)
    if get('dash-operand.status')!=b'0\n' or get('dash-operand.out')!=expected['replace']:
        raise RuntimeError('relative --version source was not applied correctly')
    for name in failures:
        expected_status=b'1\n' if name=='failed-hunk' else b'2\n'
        if get('fail-'+name+'.status')!=expected_status:raise RuntimeError('bad status: '+name)
    hits=[];families=set()
    for index in (0,1,2,4,8,16,32):
        status=get(f'alloc-{index}.status');err=get(f'alloc-{index}.err')
        marker=re.search(rb'PATCH_ALLOC_FAIL (malloc|calloc|realloc)\b',err)
        output_name=f'alloc-{index}.out'
        present=output_name in entries(image,'patch-test')
        if marker:
            hits.append(index);families.add(marker.group(1).decode())
            if status!=b'2\n' or present:raise RuntimeError(f'allocation failure {index} did not cleanly fail')
        elif index in (0,1,2,4):raise RuntimeError(f'allocation failure {index} was not observed')
        elif status!=b'0\n' or not present or get(output_name)!=expected['replace']:
            raise RuntimeError(f'allocation diagnostic {index} did not yield a valid unfailed result')
    if get('alloc-recovery.status')!=b'0\n' or get('alloc-recovery.out')!=expected['replace']:
        raise RuntimeError('patch failed to recover after allocation diagnostics')
    receipt['allocation_failure_hit_indices']=hits;receipt['allocation_failure_families_observed']=sorted(families)
    probe_log=get('probe.log')
    if get('probe.status')!=b'0\n' or b'patch-probe: PASS' not in probe_log or b'PATCH_OUTPUT_LIMIT_ARMED' not in probe_log or b'PATCH_OUTPUT_EFBIG' not in probe_log:
        raise RuntimeError('probe or output-limit marker failed')
    for source in (b'source',b'patch',b'grow'):
        pattern=rb'snapshot race remove='+source+rb' ready=1 removed=1 released=1 wait=[0-9]+ status=0'
        if len(re.findall(pattern,probe_log))!=1:raise RuntimeError('snapshot race evidence missing: '+source.decode())
    for signal,status in ((2,130),(15,143)):
        match=rb'match progress=1 signal='+str(signal).encode()+rb' wait=[0-9]+ status='+str(status).encode()+rb' output-present=0'
        output=rb'output progress=1 bytes=[1-9][0-9]* signal='+str(signal).encode()+rb' wait=[0-9]+ status='+str(status).encode()+rb' output-present=0'
        if len(re.findall(match,probe_log))!=2 or len(re.findall(output,probe_log))!=2:
            raise RuntimeError(f'signal evidence incomplete for {signal}')
    for name in ('race-source.out','race-patch.out','race-grow.out'):
        if get(name)!=expected['replace']:raise RuntimeError('snapshot race output mismatch: '+name)
    if get('reboot.status')!=b'0\n':raise RuntimeError('saved output differs after reboot')
    allowed=set(files)|set(names)|{f'alloc-{i}.out' for i in (0,1,2,4,8,16,32)}
    extras=entries(image,'patch-test')-allowed
    if extras:raise RuntimeError('unexpected output/temporary/reject artifacts: '+repr(sorted(extras)))
    receipt['status']='PASS'
    receipt['gate_results']={'two_boot_source_and_output':'PASS','host_reference_fixtures':'PASS',
        'profile_refusals_and_cleanup':'PASS','allocation_injection':'PASS',
        'snapshot_races':'PASS','signals_after_match_and_output_progress':'PASS',
        'post_admission_output_write_efbig':'PASS','guest_fsck_both_boots':'PASS'}
    receipt['pending']=['GUI Terminal','native upstream tests','two clean builds']
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    print('patch guest harness: PASS scoped checks; separate gates remain:',out,flush=True)


if __name__=='__main__':
    try:main()
    except Exception as error:
        if EVIDENCE:
            path=EVIDENCE/'receipt.json';receipt=json.loads(path.read_text()) if path.exists() else {}
            receipt.update(status='FAIL',error=f'{type(error).__name__}: {error}');path.write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
        raise
