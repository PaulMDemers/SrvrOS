#!/usr/bin/env python3
"""Two-boot bounded grep qualification against a pinned host GNU grep 3.12.

Requires a staged guest grep and test_grep_probe. Guest input refusals are
profile-specific; admitted output and statuses are compared byte-for-byte to
GNU grep in the C locale. This is not a claim that upstream tests ran in guest.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
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
from patch_smoke import directory_snapshot, entries, validate_metadata
from thread_smoke import has_fatal_exception

ROOT = Path(__file__).resolve().parents[1]
GUEST = '/fat/grep-test/'
EVIDENCE = None


def sha(data):
    return hashlib.sha256(data).hexdigest()


def fixtures():
    # All three limits are met simultaneously: 262144 bytes, 8192 lines,
    # maximum physical line length 65536 bytes (including its newline).
    envelope = b'x' * 65535 + b'\n' + (b'x' * 24 + b'\n') * 24 + (b'x' * 23 + b'\n') * (8191 - 24)
    assert len(envelope) == 262144 and envelope.count(b'\n') == 8192
    return {'basic': b'Alpha\nbeta\nALPHA\na.b\naa\nab\n-dash\nlast', 'empty': b'',
            'envelope': envelope, 'line-no-lf': b'x' * 65536,
            'overline': b'x' * 65536 + b'\n', 'overcount': b'x\n' * 8192 + b'x',
            'overbytes': (b'x' * 65535 + b'\n') * 4 + b'x',
            'nul': b'match\nlate\x00nul\n', 'work': (b'x' * 31 + b'\n') * 8192}


def cases():
    # name, options, pattern, fixture, input form
    return [('fixed', ['-F'], 'a.b', 'basic', 'file'),
            ('basic-re', [], '^a.b$', 'basic', 'file'),
            ('extended', ['-E'], 'Alpha|beta', 'basic', 'file'),
            ('ignore', ['-i', '-n'], 'alpha', 'basic', 'file'),
            ('invert', ['-v'], 'alpha', 'basic', 'file'),
            ('count', ['-c', '-i'], 'alpha', 'basic', 'file'),
            ('quiet', ['-q'], 'Alpha', 'basic', 'file'),
            ('none', [], 'absent', 'basic', 'file'),
            ('empty', [], 'x', 'empty', 'file'),
            ('no-final-lf', ['-n'], 'last', 'basic', 'file'),
            ('stdin', ['-F', '-n'], 'a', 'basic', 'stdin'),
            ('stdin-quiet', ['-q'], 'Alpha', 'basic', 'stdin'),
            ('envelope', ['-c'], '^x', 'envelope', 'file'),
            ('envelope-stdin', ['-q'], '^x', 'envelope', 'stdin'),
            ('line-no-lf', ['-q'], '^x', 'line-no-lf', 'file'),
            ('backref', [], r'^\(a\)\1$', 'basic', 'file'),
            ('empty-pattern', [], '', 'basic', 'file'),
            ('leading-dash', ['-F'], '-dash', 'basic', 'file'),
            ('pipe', ['-F', '-n'], 'a', 'basic', 'pipe')]


def references(gnu, files):
    env = dict(os.environ, LC_ALL='C', LANG='C'); refs = {}
    with tempfile.TemporaryDirectory(prefix='grep-reference-') as directory:
        root = Path(directory)
        for name, data in files.items(): (root / name).write_bytes(data)
        for name, opts, pattern, fixture, form in cases():
            argv = [gnu, *opts, '--', pattern]
            if form == 'file': argv.append(str(root / fixture))
            result = subprocess.run(argv, input=files[fixture] if form != 'file' else None,
                                    capture_output=True, env=env)
            if result.returncode not in (0, 1) or result.stderr:
                raise RuntimeError(f'GNU reference {name} failed: {result.stderr!r}')
            refs[name] = (result.returncode, result.stdout)
    return refs


def workflow():
    q = shlex.quote
    lines = ['#!/fat/bin/sh', f'chmod 755 {GUEST}grep-probe', f'chmod 755 {GUEST}ggrep-diagnostic']
    for name, opts, pattern, fixture, form in cases():
        command = ' '.join(map(q, ['ggrep', *opts, '--', pattern]))
        if form == 'file': command += ' ' + q(GUEST + fixture)
        elif form == 'stdin': command += ' < ' + q(GUEST + fixture)
        else: command = 'cat ' + q(GUEST + fixture) + ' | ' + command
        lines.extend([f'{command} > {GUEST}{name}.out 2> {GUEST}{name}.err',
                      f'echo $? > {GUEST}{name}.status'])
    failures = {
        'nul': f'-q -- match {GUEST}nul', 'nul-stdin': f'-q -- match < {GUEST}nul',
        'line': f'-q -- x {GUEST}overline', 'count': f'-q -- x {GUEST}overcount',
        'bytes': f'-q -- x {GUEST}overbytes', 'line-stdin': f'-q -- x < {GUEST}overline',
        'count-stdin': f'-q -- x < {GUEST}overcount', 'bytes-stdin': f'-q -- x < {GUEST}overbytes',
        'missing': f'-- x {GUEST}missing', 'directory': f'-- x {GUEST}',
        'unsupported': f'-r -- x {GUEST}basic', 'multiple-files': f'-- x {GUEST}basic {GUEST}basic',
        'bad-regex': f"-E -- '[' {GUEST}basic",
    }
    failures['nul-pipe'] = '-q -- match'
    for name, args in failures.items():
        prefix = f'cat {GUEST}nul | ' if name == 'nul-pipe' else ''
        lines.extend([f'{prefix}ggrep {args} > {GUEST}fail-{name}.out 2> {GUEST}fail-{name}.err',
                      f'echo $? > {GUEST}fail-{name}.status'])
    lines.extend([f'SRV_GREP_TEST_HEAP_LIMIT=0 {GUEST}ggrep-diagnostic -F -- a.b {GUEST}basic > {GUEST}heap-fail.out 2> {GUEST}heap-fail.err',
                  f'echo $? > {GUEST}heap-fail.status',
                  f'{GUEST}ggrep-diagnostic -F -- a.b {GUEST}basic > {GUEST}heap-recovery.out 2> {GUEST}heap-recovery.err',
                  f'echo $? > {GUEST}heap-recovery.status',
                  f'{GUEST}grep-probe > {GUEST}probe.log 2> {GUEST}probe.err',
                  f'echo $? > {GUEST}probe.status', 'echo GREP_WORKFLOW_DONE'])
    return ('\n'.join(lines) + '\n').encode(), list(failures)


def main():
    global EVIDENCE
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--qemu', default=os.environ.get('QEMU', 'qemu-system-x86_64'))
    ap.add_argument('--disk', type=Path, default=ROOT/'build/srvros.exfat')
    ap.add_argument('--probe', type=Path, default=ROOT/'build/userspace/test_grep_probe.elf')
    ap.add_argument('--diagnostic', type=Path, default=ROOT/'build/userspace/ggrep-diagnostic.elf')
    ap.add_argument('--gnu-grep', required=True)
    args = ap.parse_args()
    env = dict(os.environ, LC_ALL='C', LANG='C')
    version = subprocess.check_output([args.gnu_grep, '--version'], text=True, env=env).splitlines()[0]
    if not re.search(r'\bGNU grep\)? 3\.12\b', version): raise RuntimeError('GNU grep 3.12 required: ' + version)
    files = fixtures(); refs = references(args.gnu_grep, files)
    script, failures = workflow(); files['workflow.sh'] = script
    files['fixed.expected'] = refs['fixed'][1]
    files['reboot.sh'] = (b'#!/fat/bin/sh\ncmp -s /fat/grep-test/fixed.out /fat/grep-test/fixed.expected\n'
                          b'echo $? > /fat/grep-test/reboot.status\necho GREP_REBOOT_DONE\n')
    files['grep-probe'] = runtime_elf(args.probe.read_bytes())
    files['ggrep-diagnostic'] = runtime_elf(args.diagnostic.read_bytes())
    base = ROOT/'build/review/grep'; base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'), dir=base)); EVIDENCE = out
    disk = out/'disk.exfat'; shutil.copyfile(args.disk, disk); img = ExfatFixture(disk.read_bytes())
    binary = (ROOT/'build/userspace/ggrep.elf').read_bytes(); staged = img.read('bin/ggrep')
    if staged != runtime_elf(binary): raise RuntimeError('staged grep differs from build')
    baseline = directory_snapshot(img); root_before = set(baseline['root'])
    if 'grep-test' in root_before or set(baseline['sidecar']) - {'meta'}:
        raise RuntimeError('source image contains test directory or unexpected metadata sidecar')
    directory_evidence = {'before': baseline}
    (out/'directory-evidence.json').write_text(json.dumps(directory_evidence,indent=2,sort_keys=True)+'\n')
    img.add_directory_files('grep-test', files); disk.write_bytes(img.data)
    source_files = [p for p in sorted((ROOT/'ports/srvros/grep').rglob('*')) if p.is_file()]
    head_tree = subprocess.check_output(['git','ls-tree','-r','HEAD','--','ports/srvros/grep'],cwd=ROOT,text=True)
    receipt = {'status':'INCOMPLETE', 'host_grep':version, 'source_disk_sha256':sha(args.disk.read_bytes()),
               'grep_sha256':sha(binary), 'staged_grep_sha256':sha(staged),
               'kernel_sha256':sha((ROOT/'build/iso_root/boot/srvros.elf').read_bytes()),
               'initramfs_sha256':sha((ROOT/'build/iso_root/boot/initramfs.tar').read_bytes()),
               'probe_sha256':sha(files['grep-probe']), 'diagnostic_sha256':sha(files['ggrep-diagnostic']),
               'probe_elf_sha256':sha(args.probe.read_bytes()), 'diagnostic_elf_sha256':sha(args.diagnostic.read_bytes()),
               'port_head_tree':head_tree.splitlines(),
               'host_reference_binary_sha256':sha(Path(shutil.which(args.gnu_grep) or args.gnu_grep).read_bytes()),
               'port_source_sha256':{str(p.relative_to(ROOT)):sha(p.read_bytes()) for p in source_files},
               'harness_source_sha256':{str(p.relative_to(ROOT)):sha(p.read_bytes()) for p in (ROOT/'tools/grep_smoke.py',ROOT/'tools/test_grep_probe.c')},
               'fixtures_sha256':{k:sha(v) for k,v in files.items()},
               'reference_sha256':{k:sha(v[1]) for k,v in refs.items()},
               'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
               'source_status':subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True),
               'source_diff_sha256':sha(subprocess.check_output(['git','diff','--binary','HEAD'],cwd=ROOT)),
               'scope':'GNU 3.12 admitted output/status, profile refusals, held-output signals, gated admission/compile/match signals, output errors, zero-cap mapped heap refusal and fresh recovery; signals delivered at diagnostic wait; upstream scripts and GUI not run'}
    def save(): (out/'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True)+'\n')
    save(); print('grep evidence:', out, flush=True)
    with tempfile.TemporaryDirectory(prefix='grep-vm-') as temp:
        temp = Path(temp); iso = build_iso(str(ROOT),str(temp),1280,800,os.environ.copy())
        iso_root = temp/'iso_root_1280x800'/'boot'
        if (sha((iso_root/'srvros.elf').read_bytes()) != receipt['kernel_sha256'] or
            sha((iso_root/'initramfs.tar').read_bytes()) != receipt['initramfs_sha256']):
            raise RuntimeError('ISO boot files differ from recorded kernel/initramfs')
        receipt['iso_sha256']=sha(Path(iso).read_bytes()); save()
        for boot in (1,2):
            output = bytearray(); consumed = 0; port = choose_port(30001,38000); qmp_path = temp/f'qmp{boot}'
            cmd = [args.qemu,'-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
                   '-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci',
                   '-device','ide-hd,drive=disk,bus=ahci.0','-device','qemu-xhci,id=xhci',
                   '-device','usb-kbd,bus=xhci.0','-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
                   '-qmp',f'unix:{qmp_path},server=on,wait=off','-display','none','-monitor','none','-no-reboot']
            serial = qmp = None
            with (out/f'qemu-{boot}.log').open('wb') as log:
                vm = subprocess.Popen(cmd,stdout=log,stderr=log)
                try:
                    serial=connect_serial(port,20); serial.settimeout(.2); qmp=QMP(qmp_path)
                    def wait(marker, timeout):
                        nonlocal consumed
                        start=time.monotonic()
                        while marker not in output[consumed:]:
                            if marker==b'srv> ' and not consumed: qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                            if time.monotonic()-start>timeout or vm.poll() is not None: raise RuntimeError(f'boot {boot}: missing {marker!r}')
                            try:
                                chunk=serial.recv(16384)
                                if not chunk: raise RuntimeError('serial disconnected')
                                output.extend(chunk)
                            except socket.timeout: pass
                        consumed=output.index(marker,consumed)+len(marker)
                    wait(b'srv> ',100)
                    if boot == 1:
                        serial.sendall(b'run /fat/bin/sh /fat/grep-test/workflow.sh\n'); wait(b'GREP_WORKFLOW_DONE',300); wait(b'srv> ',40)
                    else:
                        serial.sendall(b'run /fat/bin/sh /fat/grep-test/reboot.sh\n')
                        wait(b'GREP_REBOOT_DONE',60); wait(b'srv> ',40)
                    serial.sendall(b'fsck /fat\n'); wait(b'exfat-check:',40); wait(b'srv> ',40)
                finally:
                    if serial: serial.close()
                    if qmp: qmp.stream.close(); qmp.sock.close()
                    stop_process(vm); (out/f'serial-{boot}.log').write_bytes(output)
            img=ExfatFixture(disk.read_bytes())
            directory_evidence[f'boot-{boot}'] = directory_snapshot(img)
            (out/'directory-evidence.json').write_text(json.dumps(directory_evidence,indent=2,sort_keys=True)+'\n')
            captured=out/f'guest-boot-{boot}'; captured.mkdir()
            names=entries(img,'grep-test') - {'.','..'}
            for name in names: (captured/name).write_bytes(img.read('grep-test/'+name))
            if not re.search(rb'exfat-check: mounts=1 files=\d+ clusters=\d+ leaks=0 fat=0 errors=0 ok',output): raise RuntimeError('guest fsck failed')
            if has_fatal_exception(output.decode(errors='replace')): raise RuntimeError('guest fatal exception')
            snap=directory_evidence[f'boot-{boot}']
            if (set(snap['root']) != root_before | {'grep-test','.srvros'} or
                snap['home'] != baseline['home'] or snap['tmp'] != baseline['tmp'] or
                set(snap['sidecar']) != {'meta'} or snap['sidecar']['meta']['attributes'] & 0x10):
                raise RuntimeError(f'boot {boot}: unexpected namespace or sidecar; see directory-evidence.json')
            receipt[f'boot_{boot}_metadata_records'] = validate_metadata(img.read('.srvros/meta'))
            expected=set(files)
            expected |= {f'{name}.{suffix}' for name,*_ in cases() for suffix in ('out','err','status')}
            expected |= {f'fail-{name}.{suffix}' for name in failures for suffix in ('out','err','status')}
            expected |= {f'{name}.{suffix}' for name in ('heap-fail','heap-recovery') for suffix in ('out','err','status')}
            expected |= {'probe.log','probe.err','probe.status','phase.out','pattern.out','pattern.err'}
            if boot == 2: expected.add('reboot.status')
            if names != expected: raise RuntimeError(f'boot {boot}: fixture names differ: missing={sorted(expected-names)} extra={sorted(names-expected)}')
            for name,data in files.items():
                if img.read('grep-test/'+name)!=data: raise RuntimeError('fixture modified: '+name)
            if boot == 2 and (captured/'fixed.out').read_bytes() != (out/'guest-boot-1/fixed.out').read_bytes():
                raise RuntimeError('saved first-boot output changed across reboot')
    img=ExfatFixture(disk.read_bytes())
    def get(name):
        data=img.read('grep-test/'+name); (out/name).write_bytes(data); return data
    for name, (status, expected) in refs.items():
        if get(name+'.status') != f'{status}\n'.encode() or get(name+'.out') != expected or get(name+'.err'):
            raise RuntimeError(name+': output, status or stderr differs from GNU 3.12')
    for name in failures:
        if get('fail-'+name+'.status') != b'2\n' or not get('fail-'+name+'.err'):
            raise RuntimeError(name+': expected status 2 and diagnostic')
    if get('reboot.status') != b'0\n': raise RuntimeError('saved output did not match expected after reboot')
    receipt['directory_evidence_sha256']=sha((out/'directory-evidence.json').read_bytes())
    if get('heap-fail.status') != b'2\n' or b'GREP_HEAP_LIMIT' not in get('heap-fail.err'):
        raise RuntimeError('mapped heap cap did not produce an observed allocation refusal')
    if get('heap-recovery.status') != b'0\n' or get('heap-recovery.err') or get('heap-recovery.out') != refs['fixed'][1]:
        raise RuntimeError('fresh diagnostic process did not recover after heap refusal')
    if get('probe.status') != b'0\n' or b'grep-probe: PASS' not in get('probe.log'): raise RuntimeError('subprocess probe failed')
    receipt['gate_results']={'gnu_reference':'PASS','profile_refusals':'PASS','second_boot':'PASS','output_errors_and_signals':'PASS', 'diagnostic_gated_phase_resume_and_signals':'PASS', 'mapped_heap_refusal_and_recovery':'PASS'}
    receipt['status']='PASS'; save(); print('grep smoke: PASS',out,flush=True)


if __name__ == '__main__':
    try: main()
    except Exception as error:
        if EVIDENCE:
            path=EVIDENCE/'receipt.json'; receipt=json.loads(path.read_text()) if path.exists() else {}
            receipt.update(status='FAIL',error=f'{type(error).__name__}: {error}')
            path.write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
        raise
