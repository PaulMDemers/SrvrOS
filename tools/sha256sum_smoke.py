#!/usr/bin/env python3
"""Disposable diskless sha256sum qualification against GNU coreutils 9.12.

Requires a staged /fat/bin/sha256sum and a matching native reference. This
checks the bounded CLI profile, not the upstream test suite. See receipt scope
for cases deliberately not claimed by this harness.
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
GUEST = '/fat/sha256sum-test/'
EVIDENCE = None


def sha(data):
    return hashlib.sha256(data).hexdigest()


def fixtures():
    data = {
        'empty': b'', 'abc': b'abc',
        'below': bytes(range(256)) * 127 + bytes(range(255)),
        'above': bytes(range(256)) * 128 + b'\x00\xff',
        'multi': bytes(range(256)) * 1024,
        '--version': b'dash operand\x00\n',
    }
    def record(name, binary=False):
        return (sha(data[name]) + (' *' if binary else '  ') + GUEST + name + '\n').encode()
    data['manifest'] = record('empty') + record('abc', True) + record('below') + record('above', True)
    data['mismatch'] = b'0' * 64 + b'  ' + (GUEST + 'abc\n').encode()
    data['missing'] = b'0' * 64 + b'  ' + (GUEST + 'absent\n').encode()
    data['malformed'] = b'not a checksum record\n'
    data['mixed'] = record('abc') + data['malformed']
    # Limits are independent: 4096 minimum-length records cannot fit 256 KiB.
    data['overbytes'] = (b'x' * 2047 + b'\n') * 128 + b'x'
    data['overrecords'] = b'\n' * 4097
    data['path1023'] = b'0' * 64 + b'  ' + b'x' * 1023 + b'\n'
    data['longpath'] = b'0' * 64 + b'  ' + b'x' * 1024 + b'\n'
    assert len(data['overbytes']) == 262145 and data['overbytes'].count(b'\n') == 128
    assert len(data['overrecords']) == 4097
    data['stdin-reuse'] = (sha(b'') + '  -\n').encode()
    return data


def cases():
    # Each tuple: name, options, operand, stdin fixture or None.
    return [
        ('empty-file', [], 'empty', None), ('abc-file', [], 'abc', None),
        ('binary', ['-b'], 'below', None), ('text', ['-t'], 'above', None),
        ('multi', [], 'multi', None),
        ('dash', [], '--version', None), ('stdin', [], None, 'above'),
        ('stdin-default', [], None, 'below'),
        ('check', ['-c'], 'manifest', None),
        ('check-stdin', ['-c'], None, 'manifest'),
    ]


def references(gnu, files):
    refs = {}
    with tempfile.TemporaryDirectory(prefix='sha256sum-reference-') as td:
        root = Path(td)
        for name, data in files.items():
            (root / name).write_bytes(data.replace(GUEST.encode(), (str(root) + '/').encode()) if name == 'manifest' else data)
        env = dict(os.environ, LC_ALL='C', LANG='C')
        for name, opts, operand, stdin in cases():
            argv = [gnu, *opts] + ([] if name == 'stdin-default' else ['--'])
            if operand is not None:
                argv.append(str(root / operand))
            elif name in ('stdin', 'check-stdin'):
                argv.append('-')
            inp = (root / stdin).read_bytes() if stdin else None
            r = subprocess.run(argv, input=inp, capture_output=True, env=env)
            if r.returncode or r.stderr:
                raise RuntimeError(f'GNU reference {name}: {r.returncode}: {r.stderr!r}')
            refs[name] = r.stdout.replace((str(root) + '/').encode(), GUEST.encode())
    return refs


def workflow():
    q = shlex.quote
    lines = ['#!/fat/bin/sh']
    for name, opts, operand, stdin in cases():
        cmd = ' '.join(map(q, ['sha256sum', *opts] + ([] if name == 'stdin-default' else ['--'])))
        if operand is not None:
            cmd += ' ' + q(GUEST + operand)
        elif name in ('stdin', 'check-stdin'):
            cmd += ' -'
        if stdin:
            # A pipe explicitly tests seekless stdin, including the manifest.
            cmd = 'cat ' + q(GUEST + stdin) + ' | ' + cmd
        lines += [f'{cmd} > {GUEST}{name}.out 2> {GUEST}{name}.err',
                  f'echo $? > {GUEST}{name}.status']
    failures = {
        'mismatch': '-c -- ' + GUEST + 'mismatch',
        'missing': '-c -- ' + GUEST + 'missing',
        'malformed': '-c -- ' + GUEST + 'malformed',
        'mixed': '-c -- ' + GUEST + 'mixed',
        'overbytes': '-c -- ' + GUEST + 'overbytes',
        'overrecords': '-c -- ' + GUEST + 'overrecords',
        'path1023': '-c -- ' + GUEST + 'path1023',
        'longpath': '-c -- ' + GUEST + 'longpath',
        'missing-file': '-- ' + GUEST + 'absent',
        'directory': '-- ' + GUEST,
        'stdin-reuse': '-c -- -',
    }
    for name, args in failures.items():
        cmd = 'sha256sum ' + args
        if name == 'stdin-reuse':
            cmd = 'cat ' + GUEST + 'stdin-reuse | ' + cmd
        lines += [f'{cmd} > {GUEST}fail-{name}.out 2> {GUEST}fail-{name}.err',
                  f'echo $? > {GUEST}fail-{name}.status']
    for option in ('help', 'version'):
        lines += [f'sha256sum --{option} > {GUEST}{option}.out 2> {GUEST}{option}.err',
                  f'echo $? > {GUEST}{option}.status']
    lines += [f'chmod 755 {GUEST}sha256sum-probe',
              f'{GUEST}sha256sum-probe > {GUEST}probe.log 2> {GUEST}probe.err',
              f'echo $? > {GUEST}probe.status', 'echo SHA256SUM_WORKFLOW_DONE']
    return ('\n'.join(lines) + '\n').encode(), failures


def main():
    global EVIDENCE
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--qemu', default=os.environ.get('QEMU', 'qemu-system-x86_64'))
    ap.add_argument('--disk', type=Path, default=ROOT / 'build/srvros.exfat')
    ap.add_argument('--binary', type=Path, default=ROOT / 'build/userspace/sha256sum.elf')
    ap.add_argument('--probe', type=Path, default=ROOT / 'build/userspace/test_sha256sum_probe.elf')
    ap.add_argument('--gnu-sha256sum', required=True)
    args = ap.parse_args()
    env = dict(os.environ, LC_ALL='C', LANG='C')
    version = subprocess.check_output([args.gnu_sha256sum, '--version'], text=True, env=env).splitlines()[0]
    if not re.search(r'\b9\.12\b', version) or 'coreutils' not in version:
        raise RuntimeError('GNU coreutils 9.12 required: ' + version)
    files = fixtures()
    refs = references(args.gnu_sha256sum, files)
    script, failures = workflow()
    files['workflow.sh'] = script
    files['sha256sum-probe'] = runtime_elf(args.probe.read_bytes())
    base = ROOT / 'build/review/sha256sum'
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'), dir=base))
    EVIDENCE = out
    disk = out / 'disk.exfat'
    shutil.copyfile(args.disk, disk)
    img = ExfatFixture(disk.read_bytes())
    binary = args.binary.read_bytes()
    staged = img.read('bin/sha256sum')
    if staged != runtime_elf(binary):
        raise RuntimeError('staged sha256sum differs from specified build')
    baseline = directory_snapshot(img)
    if 'sha256sum-test' in baseline['root'] or set(baseline['sidecar']) - {'meta'}:
        raise RuntimeError('source image contains test directory or unexpected metadata sidecar')
    img.add_directory_files('sha256sum-test', files)
    disk.write_bytes(img.data)
    source = ROOT / 'ports/srvros/sha256sum'
    receipt = {
        'status': 'INCOMPLETE', 'host_version': version,
        'host_sha256': sha(Path(shutil.which(args.gnu_sha256sum) or args.gnu_sha256sum).read_bytes()),
        'source_disk_sha256': sha(args.disk.read_bytes()), 'binary_sha256': sha(binary),
        'staged_sha256': sha(staged), 'kernel_sha256': sha((ROOT/'build/iso_root/boot/srvros.elf').read_bytes()),
        'initramfs_sha256': sha((ROOT/'build/iso_root/boot/initramfs.tar').read_bytes()),
        'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'source_status': subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
        'source_diff_sha256': sha(subprocess.check_output(['git', 'diff', '--binary', 'HEAD'], cwd=ROOT)),
        'port_sources': {str(p.relative_to(ROOT)): sha(p.read_bytes()) for p in sorted(source.rglob('*')) if p.is_file()},
        'probe_elf_sha256': sha(args.probe.read_bytes()), 'probe_source_sha256': sha((ROOT/'tools/test_sha256sum_probe.c').read_bytes()),
        'harness_sha256': sha(Path(__file__).read_bytes()), 'fixtures_sha256': {k: sha(v) for k, v in files.items()},
        'reference_sha256': {k: sha(v) for k, v in refs.items()},
        'scope': 'bounded GNU 9.12 output/status comparisons and profile refusals; pipe-consumption signal with private mapped-user snapshots, late nonblocking input error, and read-only output probe; snapshots exclude kernel/shared pages and are not RSS or peak; no internal checkpoint, late output failure, escaped-name, successful maximum-path filesystem case, reboot, GUI or upstream-suite claim',
    }
    def save():
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    save()
    print('sha256sum evidence:', out, flush=True)
    with tempfile.TemporaryDirectory(prefix='sha256sum-vm-') as td:
        temp = Path(td)
        iso = build_iso(str(ROOT), str(temp), 1280, 800, os.environ.copy())
        boot = temp / 'iso_root_1280x800' / 'boot'
        if sha((boot/'srvros.elf').read_bytes()) != receipt['kernel_sha256'] or sha((boot/'initramfs.tar').read_bytes()) != receipt['initramfs_sha256']:
            raise RuntimeError('ISO boot files differ from receipt')
        receipt['iso_sha256'] = sha(Path(iso).read_bytes()); save()
        port = choose_port(30001, 38000); qmp_path = temp / 'qmp'
        cmd = [args.qemu, '-M', 'q35', '-m', '512M', '-cdrom', str(iso), '-boot', 'd',
               '-drive', f'if=none,id=disk,file={disk},format=raw', '-device', 'ich9-ahci,id=ahci',
               '-device', 'ide-hd,drive=disk,bus=ahci.0', '-device', 'qemu-xhci,id=xhci',
               '-device', 'usb-kbd,bus=xhci.0', '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off',
               '-qmp', f'unix:{qmp_path},server=on,wait=off', '-display', 'none', '-monitor', 'none', '-no-reboot']
        output = bytearray(); consumed = 0; serial = qmp = None
        with (out/'qemu.log').open('wb') as log:
            vm = subprocess.Popen(cmd, stdout=log, stderr=log)
            try:
                serial = connect_serial(port, 20); serial.settimeout(.2); qmp = QMP(qmp_path)
                def wait(marker, timeout):
                    nonlocal consumed
                    start = time.monotonic()
                    while marker not in output[consumed:]:
                        if marker == b'srv> ' and not consumed:
                            qmp.boot_if_menu_stalled(output, time.monotonic() - start)
                        if time.monotonic()-start > timeout or vm.poll() is not None:
                            raise RuntimeError(f'missing {marker!r}')
                        try:
                            chunk = serial.recv(16384)
                            if not chunk: raise RuntimeError('serial disconnected')
                            output.extend(chunk)
                        except socket.timeout: pass
                    consumed = output.index(marker, consumed) + len(marker)
                wait(b'srv> ', 100)
                serial.sendall(b'run /fat/bin/sh /fat/sha256sum-test/workflow.sh\n')
                wait(b'SHA256SUM_WORKFLOW_DONE', 300); wait(b'srv> ', 40)
                serial.sendall(b'fsck /fat\n'); wait(b'exfat-check:', 40); wait(b'srv> ', 40)
            finally:
                if serial: serial.close()
                if qmp: qmp.stream.close(); qmp.sock.close()
                stop_process(vm); (out/'serial.log').write_bytes(output)
    if not re.search(rb'exfat-check: mounts=1 files=\d+ clusters=\d+ leaks=0 fat=0 errors=0 ok', output):
        raise RuntimeError('guest fsck failed')
    if has_fatal_exception(output.decode(errors='replace')): raise RuntimeError('guest fatal exception')
    img = ExfatFixture(disk.read_bytes()); snap = directory_snapshot(img)
    if (set(snap['root']) != set(baseline['root']) | {'sha256sum-test', '.srvros'} or
        snap['home'] != baseline['home'] or snap['tmp'] != baseline['tmp'] or
        set(snap['sidecar']) != {'meta'} or snap['sidecar']['meta']['attributes'] & 0x10):
        raise RuntimeError('unexpected namespace or sidecar')
    receipt['metadata_records'] = validate_metadata(img.read('.srvros/meta'))
    expected = set(files)
    expected |= {f'{name}.{suffix}' for name, *_ in cases() for suffix in ('out', 'err', 'status')}
    expected |= {f'fail-{name}.{suffix}' for name in failures for suffix in ('out', 'err', 'status')}
    expected |= {f'{name}.{suffix}' for name in ('help', 'version') for suffix in ('out', 'err', 'status')}
    expected |= {'probe.log', 'probe.err', 'probe.status'}
    names = entries(img, 'sha256sum-test') - {'.', '..'}
    if names != expected: raise RuntimeError(f'fixture names differ: missing={sorted(expected-names)} extra={sorted(names-expected)}')
    captured = out / 'guest'; captured.mkdir()
    for name in names: (captured/name).write_bytes(img.read('sha256sum-test/' + name))
    for name, data in files.items():
        if (captured/name).read_bytes() != data: raise RuntimeError('fixture modified: ' + name)
    for name, expected_output in refs.items():
        if ((captured/(name+'.status')).read_bytes() != b'0\n' or
            (captured/(name+'.out')).read_bytes() != expected_output or (captured/(name+'.err')).read_bytes()):
            raise RuntimeError(name + ': differs from GNU reference')
    for name in failures:
        if (captured/('fail-'+name+'.status')).read_bytes() != b'1\n' or not (captured/('fail-'+name+'.err')).read_bytes():
            raise RuntimeError(name + ': expected failure and diagnostic')
    for name, marker in {'overbytes': b'manifest exceeds 262144 bytes',
                         'overrecords': b'manifest exceeds 4096 records'}.items():
        if marker not in (captured/('fail-'+name+'.err')).read_bytes():
            raise RuntimeError(name + ': missing specific limit diagnostic')
    if (GUEST + 'abc: OK\n').encode() not in (captured/'fail-mixed.out').read_bytes():
        raise RuntimeError('mixed: valid record was not checked before strict failure')
    if b'FAILED open or read' not in (captured/'fail-path1023.out').read_bytes():
        raise RuntimeError('1023-byte decoded path was not admitted for file lookup')
    if (b'FAILED open or read' in (captured/'fail-longpath.out').read_bytes() or
        b'properly formatted checksum lines' not in (captured/'fail-longpath.err').read_bytes()):
        raise RuntimeError('1024-byte decoded path was not rejected as malformed')
    for name in ('help', 'version'):
        if (captured/(name+'.status')).read_bytes() != b'0\n' or not (captured/(name+'.out')).read_bytes() or (captured/(name+'.err')).read_bytes():
            raise RuntimeError(name + ': expected successful output')
    if (captured/'probe.status').read_bytes() != b'0\n' or b'sha256sum-probe: PASS' not in (captured/'probe.log').read_bytes():
        raise RuntimeError('guest probe failed')
    receipt['status'] = 'PASS'; save(); print('sha256sum smoke: PASS', out, flush=True)


if __name__ == '__main__':
    try: main()
    except Exception as error:
        if EVIDENCE:
            path = EVIDENCE / 'receipt.json'
            receipt = json.loads(path.read_text()) if path.exists() else {}
            receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
            path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
        raise
