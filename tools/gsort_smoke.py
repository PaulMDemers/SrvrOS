#!/usr/bin/env python3
"""Qualify bounded gsort against native GNU sort 9.12 in the C locale.
"""
import argparse
import hashlib
import json
import os
import re
import shlex
import shutil
import socket
import subprocess
import tempfile
import time
from pathlib import Path
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from patch_smoke import directory_snapshot, entries, validate_metadata
from thread_smoke import has_fatal_exception
ROOT = Path(__file__).resolve().parents[1]
GUEST = '/fat/gsort-test/'
EVIDENCE = None

def sha(data):
    return hashlib.sha256(data).hexdigest()

def fixtures():
    return {
        'tiny': b'z\na\n',
        'ties': b'2 zebra\n02 apple\n2 pear\n-1 tail\n0 zero\n',
        'fields': b'b:02:pear\na:2:apple\nc:-1:plum\nd:2:apricot\n',
        'whitespace': b'z  beta\na alpha\nx   alpha\nb\tzed\n',
        'missing-fields': b'z::tail\na:alpha\nb\nc::\nd:beta:tail\n',
        'high-bytes': b'\xffz\n\x80a\n\x7fz\na\n\xfe\n',
        'decimals': (b'1.00000000000000000000000000000001 alpha\n'
                     b'1.00000000000000000000000000000002 beta\n'
                     b'+3.5 plus\n-0.125 minus\n.125 fraction\n'
                     b'0001.00000000000000000000000000000001 tied\n'),
        'long-lf': b'z' * 65535 + b'\n' + b'a\n',
        'long-eof': b'z' * 65536,
        'count': b'x\n' * 8192,
        'count-over': b'x\n' * 8193,
        'bytes': (b'x' * 31 + b'\n') * 8192,
        'bytes-over': (b'x' * 31 + b'\n') * 8191 + b'y' * 32 + b'\n',
        'record-over': b'x' * 65537,
        'record-lf-over': b'x' * 65536 + b'\n',
        'nul': b'hello\x00world\n',
    }

def cases():
    return {
        'plain': ('tiny', [], False),
        'stdin': ('ties', ['-n'], True),
        'explicit-stdin': ('ties', ['-n', '-'], True),
        'numeric': ('ties', ['-n'], False),
        'numeric-unique': ('ties', ['-n', '-u'], False),
        'numeric-stable-unique': ('ties', ['-n', '-s', '-u'], False),
        'reverse': ('ties', ['-n', '-r'], False),
        'key': ('fields', ['-n', '-t', ':', '-k', '2,2'], False),
        'key-stable': ('fields', ['-n', '-s', '-t', ':', '-k', '2,2'], False),
        'whitespace-key': ('whitespace', ['-k', '2,2'], False),
        'missing-key': ('missing-fields', ['-t', ':', '-k', '2,2'], False),
        'high-bytes': ('high-bytes', [], False),
        'decimals': ('decimals', ['-n'], False),
        'long-lf': ('long-lf', [], False),
        'long-eof': ('long-eof', [], False),
        'count': ('count', [], False),
        'bytes': ('bytes', [], False),
    }

def references(reference, files):
    version = subprocess.run([reference, '--version'], capture_output=True, env=dict(os.environ, LC_ALL='C', LANG='C'))
    if version.returncode or not re.search(rb'(?m)^sort .* 9\.12(?:\s|$)', version.stdout):
        raise RuntimeError('reference must be GNU sort 9.12: ' + repr(version.stdout[:200]))
    refs = {}
    for tag, (name, opts, _) in cases().items():
        opts = [x for x in opts if x != '-']
        result = subprocess.run([reference, *opts], input=files[name], capture_output=True, env=dict(os.environ, LC_ALL='C', LANG='C'))
        if result.returncode or result.stderr: raise RuntimeError(f'reference {tag}: {result.returncode} {result.stderr!r}')
        refs[tag] = result.stdout
    return refs

def workflow(files):
    q = shlex.quote
    lines = ['#!/fat/bin/sh']
    for tag, (name, opts, stdin) in cases().items():
        cmd = ' '.join(map(q, ['gsort', *opts] + ([] if stdin else [GUEST + name])))
        if stdin: cmd += ' < ' + GUEST + name
        lines += [f'{cmd} > {GUEST}{tag}.out 2> {GUEST}{tag}.err', f'echo $? > {GUEST}{tag}.status']
    failures = {
        'count-over': GUEST + 'count-over', 'bytes-over': GUEST + 'bytes-over',
        'record-over': GUEST + 'record-over', 'record-lf-over': GUEST + 'record-lf-over', 'nul': GUEST + 'nul',
        'missing': GUEST + 'missing', 'directory': GUEST,
        'multiple': GUEST + 'tiny ' + GUEST + 'ties',
        'bad-option': '--not-an-option', 'bad-key': '-k 1,2 ' + GUEST + 'tiny',
        'bad-delimiter': '-t ab ' + GUEST + 'tiny', 'output-option': '-o ' + GUEST + 'tiny',
    }
    for name, args in failures.items():
        lines += [f'gsort {args} > {GUEST}fail-{name}.out 2> {GUEST}fail-{name}.err', f'echo $? > {GUEST}fail-{name}.status']
    lines += [f'chmod 755 {GUEST}gsort-probe', f'chmod 755 {GUEST}gsort-diagnostic',
        f'{GUEST}gsort-probe > {GUEST}probe.log 2> {GUEST}probe.err', f'echo $? > {GUEST}probe.status',
        f'SRV_GSORT_HEAP=1 {GUEST}gsort-diagnostic {GUEST}bytes > {GUEST}heap-limit.out 2> {GUEST}heap-limit.err', f'echo $? > {GUEST}heap-limit.status',
        f'SRV_GSORT_MAX_HEAP=0 {GUEST}gsort-diagnostic {GUEST}tiny > {GUEST}heap-fail.out 2> {GUEST}heap-fail.err', f'echo $? > {GUEST}heap-fail.status',
        f'{GUEST}gsort-diagnostic {GUEST}tiny > {GUEST}heap-retry.out 2> {GUEST}heap-retry.err', f'echo $? > {GUEST}heap-retry.status',
        'echo GSORT_WORKFLOW_DONE']
    return ('\n'.join(lines) + '\n').encode(), failures

def main():
    global EVIDENCE
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--qemu', default=os.environ.get('QEMU', 'qemu-system-x86_64'))
    ap.add_argument('--disk', type=Path, default=ROOT / 'build/srvros.exfat')
    ap.add_argument('--binary', type=Path, default=ROOT / 'build/userspace/gsort.elf')
    ap.add_argument('--diagnostic', type=Path, default=ROOT / 'build/userspace/gsort-diagnostic.elf')
    ap.add_argument('--probe', type=Path, default=ROOT / 'build/userspace/test_gsort_probe.elf')
    ap.add_argument('--reference', required=True, help='native GNU sort 9.12 executable')
    args = ap.parse_args()
    files = fixtures()
    refs = references(args.reference, files)
    script, failures = workflow(files)
    files['workflow.sh'] = script
    files['gsort-probe'] = runtime_elf(args.probe.read_bytes())
    files['gsort-diagnostic'] = runtime_elf(args.diagnostic.read_bytes())
    base = ROOT / 'build/review/gsort'
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'), dir=base))
    EVIDENCE = out
    disk = out / 'disk.exfat'
    shutil.copyfile(args.disk, disk)
    img = ExfatFixture(disk.read_bytes())
    binary = args.binary.read_bytes()
    staged = img.read('bin/gsort')
    if staged != runtime_elf(binary):
        raise RuntimeError('staged gsort differs from specified build')
    baseline = directory_snapshot(img)
    if 'gsort-test' in baseline['root'] or set(baseline['sidecar']) - {'meta'}:
        raise RuntimeError('source image contains test directory or unexpected metadata sidecar')
    img.add_directory_files('gsort-test', files)
    disk.write_bytes(img.data)
    receipt = {
        'status': 'INCOMPLETE',
        'reference_sha256': sha(Path(args.reference).read_bytes()),
        'source_disk_sha256': sha(args.disk.read_bytes()),
        'binary_sha256': sha(binary),
        'staged_sha256': sha(staged),
        'diagnostic_sha256': sha(args.diagnostic.read_bytes()),
        'probe_sha256': sha(args.probe.read_bytes()),
        'kernel_sha256': sha((ROOT / 'build/iso_root/boot/srvros.elf').read_bytes()),
        'initramfs_sha256': sha((ROOT / 'build/iso_root/boot/initramfs.tar').read_bytes()),
        'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'source_status': subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
        'source_diff_sha256': sha(subprocess.check_output(['git', 'diff', '--binary', 'HEAD'], cwd=ROOT)),
        'harness_sha256': sha(Path(__file__).read_bytes()),
        'probe_source_sha256': sha((ROOT / 'tools/test_gsort_probe.c').read_bytes()),
        'fixtures_sha256': {k: sha(v) for k, v in files.items()},
        'reference_sha256s': {k: sha(v) for k, v in refs.items()},
        'scope': 'C locale bounded in-memory sort; mapped heap is not RSS or peak',
    }

    def save():
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    save()
    print('gsort evidence:', out, flush=True)
    with tempfile.TemporaryDirectory(prefix='gsort-vm-') as td:
        temp = Path(td)
        iso = build_iso(str(ROOT), str(temp), 1280, 800, os.environ.copy())
        boot = temp / 'iso_root_1280x800' / 'boot'
        if sha((boot / 'srvros.elf').read_bytes()) != receipt['kernel_sha256'] or sha((boot / 'initramfs.tar').read_bytes()) != receipt['initramfs_sha256']:
            raise RuntimeError('ISO boot files differ from receipt')
        receipt['iso_sha256'] = sha(Path(iso).read_bytes())
        save()
        port = choose_port(30001, 38000)
        qmp_path = temp / 'qmp'
        cmd = [
            args.qemu,
            '-M',
            'q35',
            '-m',
            '512M',
            '-cdrom',
            str(iso),
            '-boot',
            'd',
            '-drive',
            f'if=none,id=disk,file={disk},format=raw',
            '-device',
            'ich9-ahci,id=ahci',
            '-device',
            'ide-hd,drive=disk,bus=ahci.0',
            '-device',
            'qemu-xhci,id=xhci',
            '-device',
            'usb-kbd,bus=xhci.0',
            '-serial',
            f'tcp:127.0.0.1:{port},server=on,wait=off',
            '-qmp',
            f'unix:{qmp_path},server=on,wait=off',
            '-display',
            'none',
            '-monitor',
            'none',
            '-no-reboot',
        ]
        output = bytearray()
        consumed = 0
        serial = qmp = None
        with (out / 'qemu.log').open('wb') as log:
            vm = subprocess.Popen(cmd, stdout=log, stderr=log)
            try:
                serial = connect_serial(port, 20)
                serial.settimeout(0.2)
                qmp = QMP(qmp_path)

                def wait(marker, timeout):
                    nonlocal consumed
                    start = time.monotonic()
                    while marker not in output[consumed:]:
                        if marker == b'srv> ' and (not consumed):
                            qmp.boot_if_menu_stalled(output, time.monotonic() - start)
                        if time.monotonic() - start > timeout or vm.poll() is not None:
                            raise RuntimeError(f'missing {marker!r}')
                        try:
                            chunk = serial.recv(16384)
                            if not chunk:
                                raise RuntimeError('serial disconnected')
                            output.extend(chunk)
                        except socket.timeout:
                            pass
                    consumed = output.index(marker, consumed) + len(marker)
                wait(b'srv> ', 100)
                serial.sendall(b'run /fat/bin/sh /fat/gsort-test/workflow.sh\n')
                wait(b'GSORT_WORKFLOW_DONE', 300)
                wait(b'srv> ', 40)
                serial.sendall(b'fsck /fat\n')
                wait(b'exfat-check:', 40)
                wait(b'srv> ', 40)
            finally:
                if serial:
                    serial.close()
                if qmp:
                    qmp.stream.close()
                    qmp.sock.close()
                stop_process(vm)
                (out / 'serial.log').write_bytes(output)
    if not re.search(b'exfat-check: mounts=1 files=\\d+ clusters=\\d+ leaks=0 fat=0 errors=0 ok', output):
        raise RuntimeError('guest fsck failed')
    if has_fatal_exception(output.decode(errors='replace')):
        raise RuntimeError('guest fatal exception')
    img = ExfatFixture(disk.read_bytes())
    snap = directory_snapshot(img)
    if set(snap['root']) != set(baseline['root']) | {'gsort-test', '.srvros'} or snap['home'] != baseline['home'] or snap['tmp'] != baseline['tmp'] or (set(snap['sidecar']) != {'meta'}) or snap['sidecar']['meta']['attributes'] & 16:
        raise RuntimeError('unexpected namespace or sidecar')
    receipt['metadata_records'] = validate_metadata(img.read('.srvros/meta'))
    expected = set(files)
    expected |= {f'{name}.{suffix}' for name in refs for suffix in ('out', 'err', 'status')}
    expected |= {f'fail-{name}.{suffix}' for name in failures for suffix in ('out', 'err', 'status')}
    expected |= {'probe.log', 'probe.err', 'probe.status'}
    expected |= {f'heap-{name}.{suffix}' for name in ('limit', 'fail', 'retry') for suffix in ('out', 'err', 'status')}
    expected |= {'repeat.out', 'retry.out'}
    names = entries(img, 'gsort-test') - {'.', '..'}
    if names != expected:
        raise RuntimeError(f'fixture names differ: missing={sorted(expected - names)} extra={sorted(names - expected)}')
    captured = out / 'guest'
    captured.mkdir()
    for name in names:
        (captured / name).write_bytes(img.read('gsort-test/' + name))
    for name, data in files.items():
        if (captured / name).read_bytes() != data:
            raise RuntimeError('fixture modified: ' + name)
    for name, expected_output in refs.items():
        if (captured / (name + '.status')).read_bytes() != b'0\n' or (captured / (name + '.out')).read_bytes() != expected_output or (captured / (name + '.err')).read_bytes():
            raise RuntimeError(name + ': differs from bounded native reference')
    for name in failures:
        if (captured / ('fail-' + name + '.status')).read_bytes() != b'2\n' or not (captured / ('fail-' + name + '.err')).read_bytes() or (captured / ('fail-' + name + '.out')).read_bytes():
            raise RuntimeError(name + ': expected status 2, diagnostic and empty output')
    if (captured / 'probe.status').read_bytes() != b'0\n' or b'gsort-probe: PASS' not in (captured / 'probe.log').read_bytes():
        raise RuntimeError('guest process probe failed')
    if (captured / 'retry.out').read_bytes() != refs['plain']:
        raise RuntimeError('probe retry output differs')
    heap = re.search(rb'GSORT_HEAP=(\d+)', (captured / 'heap-limit.err').read_bytes())
    if (captured / 'heap-limit.status').read_bytes() != b'0\n' or not heap or not 0 < int(heap[1]) <= 16*1024*1024 or (captured / 'heap-limit.out').read_bytes() != refs['bytes']:
        raise RuntimeError('heap diagnostic failed or exceeded 16 MiB')
    if (captured / 'heap-fail.status').read_bytes() != b'2\n' or b'GSORT_HEAP_LIMIT' not in (captured / 'heap-fail.err').read_bytes() or (captured / 'heap-fail.out').read_bytes():
        raise RuntimeError('forced heap limit did not fail cleanly')
    if (captured / 'heap-retry.status').read_bytes() != b'0\n' or (captured / 'heap-retry.err').read_bytes() or (captured / 'heap-retry.out').read_bytes() != refs['plain']:
        raise RuntimeError('retry after heap failure failed')
    receipt['heap_mapped_bytes'] = int(heap[1])
    receipt['status'] = 'PASS'
    save()
    print('gsort smoke: PASS', out, flush=True)
if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        if EVIDENCE:
            path = EVIDENCE / 'receipt.json'
            receipt = json.loads(path.read_text()) if path.exists() else {}
            receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
            path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
        raise
