#!/usr/bin/env python3
"""Qualify bounded file-magic against a matching native magic_buffer driver.

The reference executable takes DATABASE followed by input paths and emits three
lines per input: path, two-space-indented description, and MIME type. It must
use the same database and NO_CHECK_COMPRESS|NO_CHECK_ELF|NO_CHECK_CDF flags.
"""
import argparse
import gzip
import hashlib
import io
import json
import lzma
import os
import re
import shlex
import shutil
import socket
import sqlite3
import subprocess
import tarfile
import tempfile
import time
import zipfile
from pathlib import Path
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from patch_smoke import directory_snapshot, entries, validate_metadata
from thread_smoke import has_fatal_exception
ROOT = Path(__file__).resolve().parents[1]
GUEST = '/fat/file-magic-test/'
EVIDENCE = None

def sha(data):
    return hashlib.sha256(data).hexdigest()

def fixtures():
    z = io.BytesIO()
    with zipfile.ZipFile(z, 'w') as archive:
        archive.writestr(zipfile.ZipInfo('hello.txt', (2020, 1, 1, 0, 0, 0)), b'hello\n')
    with tempfile.TemporaryDirectory(prefix='file-magic-sqlite-') as td:
        db = Path(td) / 'fixture.sqlite'
        with sqlite3.connect(db) as conn:
            conn.execute('CREATE TABLE sample (value TEXT)')
            conn.execute("INSERT INTO sample VALUES ('hello')")
        sqlite = db.read_bytes()
    t = io.BytesIO()
    with tarfile.open(fileobj=t, mode='w') as archive:
        info = tarfile.TarInfo('hello.txt')
        info.size = 6
        info.mtime = 0
        archive.addfile(info, io.BytesIO(b'hello\n'))
    prefix = b'X' * 262144
    return {
        'empty': b'',
        'plain': b'plain text\n',
        'unknown': bytes(512),
        'elf': b'\x7fELF' + bytes([2, 1, 1]) + bytes(57),
        'png': bytes.fromhex('89504e470d0a1a0a0000000d4948445200000001000000010802000000907753de'),
        'sqlite': sqlite,
        'zip': z.getvalue(),
        'gzip': gzip.compress(b'hello\n', mtime=0),
        'xz': lzma.compress(b'hello\n'),
        'tar': t.getvalue(),
        'limit': prefix,
        'limit-tail': prefix + b'\x00\xffextra',
        '--dash': b'plain text\n',
    }

def references(reference, database, files):
    refs = {}
    with tempfile.TemporaryDirectory(prefix='file-magic-reference-') as td:
        root = Path(td)
        for name, data in files.items():
            (root / name).write_bytes(data)
        for name in files:
            path = root / name
            r = subprocess.run([reference, str(database), str(path)], capture_output=True, env=dict(os.environ, LC_ALL='C', LANG='C'))
            lines = r.stdout.splitlines()
            if r.returncode or r.stderr or len(lines) != 3 or (lines[0] != str(path).encode()) or (not all((x.startswith(b'  ') for x in lines[1:]))):
                raise RuntimeError(f'reference {name}: {r.returncode}, {r.stdout!r}, {r.stderr!r}')
            desc, mime = (lines[1][2:], lines[2][2:])
            refs[name + '-default'] = (GUEST + name + ': ').encode() + desc + b'\n'
            refs[name + '-brief'] = desc + b'\n'
            refs[name + '-mime'] = mime + b'\n'
            refs[name + '-mime-default'] = (GUEST + name + ': ').encode() + mime + b'\n'
        if refs['limit-brief'] != refs['limit-tail-brief'] or refs['limit-mime'] != refs['limit-tail-mime']:
            raise RuntimeError('reference failed 256 KiB bound')
    return refs

def workflow(files):
    q = shlex.quote
    lines = ['#!/fat/bin/sh']
    for name in files:
        for mode, opts in [('default', []), ('brief', ['-b']), ('mime', ['-b', '--mime-type']), ('mime-default', ['--mime-type'])]:
            tag = name + '-' + mode
            cmd = ' '.join(map(q, ['file-magic', *opts, '--', GUEST + name]))
            lines += [f'{cmd} > {GUEST}{tag}.out 2> {GUEST}{tag}.err', f'echo $? > {GUEST}{tag}.status']
    failures = {
        'missing': '-- ' + GUEST + 'absent',
        'directory': '-- ' + GUEST,
        'multiple': '-- ' + GUEST + 'png ' + GUEST + 'plain',
        'no-operand': '',
        'unknown-option': '--not-an-option',
    }
    for name, args in failures.items():
        lines += [f'file-magic {args} > {GUEST}fail-{name}.out 2> {GUEST}fail-{name}.err', f'echo $? > {GUEST}fail-{name}.status']
    lines += [
        f'chmod 755 {GUEST}file-magic-probe',
        f'chmod 755 {GUEST}file-magic-diagnostic',
        f'{GUEST}file-magic-probe > {GUEST}probe.log 2> {GUEST}probe.err',
        f'echo $? > {GUEST}probe.status',
        f'SRV_FILE_MAGIC_HEAP=1 {GUEST}file-magic-diagnostic -b -- {GUEST}png > {GUEST}heap-png.out 2> {GUEST}heap-png.err',
        f'echo $? > {GUEST}heap-png.status',
        f'SRV_FILE_MAGIC_HEAP=1 {GUEST}file-magic-diagnostic -b -- {GUEST}limit > {GUEST}heap-limit.out 2> {GUEST}heap-limit.err',
        f'echo $? > {GUEST}heap-limit.status',
        f'SRV_FILE_MAGIC_MAX_HEAP=0 {GUEST}file-magic-diagnostic -b -- {GUEST}png > {GUEST}heap-fail.out 2> {GUEST}heap-fail.err',
        f'echo $? > {GUEST}heap-fail.status',
        f'{GUEST}file-magic-diagnostic -b -- {GUEST}png > {GUEST}heap-retry.out 2> {GUEST}heap-retry.err',
        f'echo $? > {GUEST}heap-retry.status',
        'echo FILE_MAGIC_WORKFLOW_DONE',
    ]
    return (('\n'.join(lines) + '\n').encode(), failures)

def main():
    global EVIDENCE
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--qemu', default=os.environ.get('QEMU', 'qemu-system-x86_64'))
    ap.add_argument('--disk', type=Path, default=ROOT / 'build/srvros.exfat')
    ap.add_argument('--binary', type=Path, default=ROOT / 'build/userspace/file-magic.elf')
    ap.add_argument('--diagnostic', type=Path, default=ROOT / 'build/userspace/file-magic-diagnostic.elf')
    ap.add_argument('--probe', type=Path, default=ROOT / 'build/userspace/test_file_magic_probe.elf')
    ap.add_argument('--reference', required=True, help='matching native magic_buffer driver')
    ap.add_argument('--database', type=Path, required=True, help='same compiled magic database as guest')
    args = ap.parse_args()
    files = fixtures()
    refs = references(args.reference, args.database, files)
    script, failures = workflow(files)
    files['workflow.sh'] = script
    files['file-magic-probe'] = runtime_elf(args.probe.read_bytes())
    files['file-magic-diagnostic'] = runtime_elf(args.diagnostic.read_bytes())
    base = ROOT / 'build/review/file-magic'
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'), dir=base))
    EVIDENCE = out
    disk = out / 'disk.exfat'
    shutil.copyfile(args.disk, disk)
    img = ExfatFixture(disk.read_bytes())
    binary = args.binary.read_bytes()
    staged = img.read('bin/file-magic')
    if staged != runtime_elf(binary):
        raise RuntimeError('staged file-magic differs from specified build')
    database = args.database.read_bytes()
    if not database or staged.count(database) != 1:
        raise RuntimeError('staged executable does not contain exactly one copy of reference database')
    diagnostic = files['file-magic-diagnostic']
    if diagnostic.count(database) != 1:
        raise RuntimeError('diagnostic does not contain exactly one copy of reference database')
    baseline = directory_snapshot(img)
    if 'file-magic-test' in baseline['root'] or set(baseline['sidecar']) - {'meta'}:
        raise RuntimeError('source image contains test directory or unexpected metadata sidecar')
    img.add_directory_files('file-magic-test', files)
    disk.write_bytes(img.data)
    receipt = {
        'status': 'INCOMPLETE',
        'reference_sha256': sha(Path(args.reference).read_bytes()),
        'database_sha256': sha(args.database.read_bytes()),
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
        'probe_source_sha256': sha((ROOT / 'tools/test_file_magic_probe.c').read_bytes()),
        'fixtures_sha256': {k: sha(v) for k, v in files.items()},
        'reference_sha256s': {k: sha(v) for k, v in refs.items()},
        'scope': 'bounded 256 KiB magic_buffer; paired limit inputs share their first 256 KiB; this pair does not establish that an uncapped classifier differs. No compression, ELF or CDF deep checks; mapped heap is not RSS or peak',
    }

    def save():
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    save()
    print('file-magic evidence:', out, flush=True)
    with tempfile.TemporaryDirectory(prefix='file-magic-vm-') as td:
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
                serial.sendall(b'run /fat/bin/sh /fat/file-magic-test/workflow.sh\n')
                wait(b'FILE_MAGIC_WORKFLOW_DONE', 300)
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
    if set(snap['root']) != set(baseline['root']) | {'file-magic-test', '.srvros'} or snap['home'] != baseline['home'] or snap['tmp'] != baseline['tmp'] or (set(snap['sidecar']) != {'meta'}) or snap['sidecar']['meta']['attributes'] & 16:
        raise RuntimeError('unexpected namespace or sidecar')
    receipt['metadata_records'] = validate_metadata(img.read('.srvros/meta'))
    expected = set(files)
    expected |= {f'{name}.{suffix}' for name in refs for suffix in ('out', 'err', 'status')}
    expected |= {f'fail-{name}.{suffix}' for name in failures for suffix in ('out', 'err', 'status')}
    expected |= {'probe.log', 'probe.err', 'probe.status'}
    expected |= {f'heap-{name}.{suffix}' for name in ('png', 'limit', 'fail', 'retry') for suffix in ('out', 'err', 'status')}
    names = entries(img, 'file-magic-test') - {'.', '..'}
    if names != expected:
        raise RuntimeError(f'fixture names differ: missing={sorted(expected - names)} extra={sorted(names - expected)}')
    captured = out / 'guest'
    captured.mkdir()
    for name in names:
        (captured / name).write_bytes(img.read('file-magic-test/' + name))
    for name, data in files.items():
        if (captured / name).read_bytes() != data:
            raise RuntimeError('fixture modified: ' + name)
    for name, expected_output in refs.items():
        if (captured / (name + '.status')).read_bytes() != b'0\n' or (captured / (name + '.out')).read_bytes() != expected_output or (captured / (name + '.err')).read_bytes():
            raise RuntimeError(name + ': differs from bounded native reference')
    for name in failures:
        if (captured / ('fail-' + name + '.status')).read_bytes() != b'2\n' or not (captured / ('fail-' + name + '.err')).read_bytes():
            raise RuntimeError(name + ': expected status 2 and diagnostic')
    if (captured / 'probe.status').read_bytes() != b'0\n' or b'file-magic-probe: PASS' not in (captured / 'probe.log').read_bytes():
        raise RuntimeError('guest process probe failed')
    heaps = {}
    for name in ('png', 'limit'):
        heap = re.fullmatch(b'FILE_MAGIC_HEAP=(\\d+)\\n', (captured / f'heap-{name}.err').read_bytes())
        if (captured / f'heap-{name}.status').read_bytes() != b'0\n' or not heap or (not 0 < int(heap[1]) <= 16 * 1024 * 1024):
            raise RuntimeError(name + ': heap diagnostic missing or over 16 MiB')
        if (captured / f'heap-{name}.out').read_bytes() != refs[name + '-brief']:
            raise RuntimeError(name + ': heap diagnostic changed classification')
        heaps[name] = int(heap[1])
    if (captured / 'heap-fail.status').read_bytes() != b'2\n' or b'FILE_MAGIC_HEAP_LIMIT' not in (captured / 'heap-fail.err').read_bytes() or (captured / 'heap-fail.out').read_bytes():
        raise RuntimeError('forced heap limit did not fail cleanly')
    if (captured / 'heap-retry.status').read_bytes() != b'0\n' or (captured / 'heap-retry.err').read_bytes() or (captured / 'heap-retry.out').read_bytes() != refs['png-brief']:
        raise RuntimeError('classification retry after heap failure failed')
    receipt['heap_mapped_bytes'] = heaps
    receipt['status'] = 'PASS'
    save()
    print('file-magic smoke: PASS', out, flush=True)
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
