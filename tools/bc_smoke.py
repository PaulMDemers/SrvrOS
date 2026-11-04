#!/usr/bin/env python3
"""Exercise the staged bc CLI on a disposable disk across two guest boots.

Requires the checkout's built ISO, exFAT image, probe ELFs and exfat-driver.
No build or staging is performed. Evidence is saved under build/bc-smoke.
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
from exfat_test_image import ExfatFixture
from thread_smoke import has_fatal_exception
from ports_smoke import send_serial_line
from pack_runtime_elf import runtime_elf

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ('add', 'subtract', 'multiply', 'divide', 'sqrt', 'functions', 'assignments')
FILES = {
    'core.bc': b"123456789012345678901234567890+987654321098765432109876543210\nscale=5; 1/8; sqrt(2)\nobase=16; 255; obase=10\nibase=16; FF; ibase=A\na=3; define f(x) { return(x*x) }; f(a)\nfor(i=1;i<=4;i++) a+=i\na\n",
    'totals.bc': b"scale=2\nsubtotal=19.95*3+4.50*2\ntax=subtotal*0.08\nsubtotal\ntax\nsubtotal+tax\n",
    'scale.bc': b"scale=20\n",
    'empty': b'',
    'bad.bc': b"1+*2\n",
    'divide.bc': b"1/0\n",
    'workflow.sh': b'''#!/fat/bin/sh
cd /fat/bc-test
bc --version > version.txt 2> version-error.txt
echo $? > version-status.txt
bc core.bc < empty > core.txt 2> core-error.txt
echo $? > core-status.txt
bc < core.bc > stdin.txt 2> stdin-error.txt
echo $? > stdin-status.txt
cat core.bc | bc > pipe.txt 2> pipe-error.txt
echo $? > pipe-status.txt
bc totals.bc < empty > totals.txt 2> totals-error.txt
echo $? > totals-status.txt
bc bad.bc < empty > syntax.txt 2> syntax-error.txt
echo $? > syntax-status.txt
bc divide.bc < empty > divide.txt 2> divide-error.txt
echo $? > divide-status.txt
bc absent.bc < empty > missing.txt 2> missing-error.txt
echo $? > missing-status.txt
chmod 755 /fat/bc-test/bc-output-test
chmod 755 /fat/bc-test/bc-signal-test
/fat/bc-test/bc-output-test > output-probe.txt 2> output-probe-error.txt
echo $? > output-probe-status.txt
/fat/bc-test/bc-signal-test > signal-probe.txt 2> signal-probe-error.txt
echo $? > signal-probe-status.txt
bc totals.bc < empty > recovery.txt 2> recovery-error.txt
echo $? > recovery-status.txt
''' + b''.join(f'bc scale.bc upstream-{n}.txt < empty > upstream-{n}-out.txt 2> upstream-{n}-error.txt\necho $? > upstream-{n}-status.txt\n'.encode() for n in UPSTREAM),
    'reopen.sh': b'''#!/fat/bin/sh
cd /fat/bc-test
cat totals.txt > reopen.txt
echo $? > reopen-status.txt
''',
}
CORE = b'1111111110111111111011111111100\n.12500\n1.41421\nFF\n255\n9\n13\n'
TOTALS = b'68.85\n5.50\n74.35\n'
EXPECTED = {
    'core.txt': CORE, 'stdin.txt': CORE, 'pipe.txt': CORE,
    'totals.txt': TOTALS, 'recovery.txt': TOTALS, 'reopen.txt': TOTALS,
    **{name + '-status.txt': b'0\n' for name in ('version', 'core', 'stdin', 'pipe', 'totals', 'recovery', 'reopen', 'output-probe', 'signal-probe')},
    **{f'upstream-{name}-status.txt': b'0\n' for name in UPSTREAM},
    'output-probe.txt': b'bc-output-error: PASS\n',
    'signal-probe.txt': b'bc-signal: PASS\n',
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path, default=ROOT / 'build/srvros-x86_64.iso')
    parser.add_argument('--disk', type=Path, default=ROOT / 'build/srvros.exfat')
    parser.add_argument('--qemu', default=os.environ.get('QEMU', 'qemu-system-x86_64'))
    parser.add_argument('--fsck', default=shutil.which('fsck.exfat'))
    args = parser.parse_args()
    iso, source = args.iso.resolve(), args.disk.resolve()
    build = (ROOT / 'build').resolve()
    for path in (iso, source):
        if not path.is_relative_to(build):
            raise RuntimeError(f'Artifact must be inside this checkout’s build directory: {path}')
    driver = ROOT / 'build/tests/exfat-driver'
    for path in (iso, source, driver):
        if not path.is_file():
            raise RuntimeError(f'Missing required build artifact: {path}')
    image = ExfatFixture(source.read_bytes())
    try:
        staged = image.read('bin/bc')
    except (StopIteration, ValueError) as error:
        raise RuntimeError('Source disk must contain /fat/bin/bc') from error
    out_root = ROOT / 'build/bc-smoke'
    out_root.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'), dir=out_root))
    disk = out / 'disk.exfat'
    payloads = dict(FILES)
    fixture_hashes = {}
    for name in UPSTREAM:
        for suffix in ('', '_results'):
            data = (ROOT / f'ports/upstream/bc/tests/bc/{name}{suffix}.txt').read_bytes()
            payloads[f'upstream-{name}{suffix}.txt'] = data
            fixture_hashes[f'{name}{suffix}'] = hashlib.sha256(data).hexdigest()
    for name in ('bc-output-test', 'bc-signal-test'):
        payloads[name] = runtime_elf((ROOT / f'build/userspace/{name}.elf').read_bytes())
    image.add_directory_files('bc-test', payloads)
    disk.write_bytes(image.data)
    receipt = {'status': 'INCOMPLETE', 'iso_sha256': sha(iso), 'source_disk_sha256': sha(source),
               'staged_bc_sha256': hashlib.sha256(staged).hexdigest(),
               'selected_upstream_sha256': fixture_hashes,
               'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'source_status': subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
               'scope': 'bc static arithmetic, errors, signal handling and reboot persistence; no dc, history, NLS or extra math'}
    receipt_path = out / 'receipt.json'
    def save():
        receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    save()
    print('bc smoke evidence:', out, flush=True)
    try:
        with tempfile.TemporaryDirectory(prefix='bc-vm-') as temp:
            for boot in (1, 2):
                port = choose_port(30001, 38000)
                qmp_path = Path(temp) / f'qmp{boot}'
                cmd = [args.qemu, '-M', 'q35', '-m', '512M', '-cdrom', str(iso), '-boot', 'd',
                       '-drive', f'if=none,id=disk,file={disk},format=raw', '-device', 'ich9-ahci,id=ahci',
                       '-device', 'ide-hd,drive=disk,bus=ahci.0', '-device', 'qemu-xhci,id=xhci',
                       '-device', 'usb-kbd,bus=xhci.0', '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off',
                       '-qmp', f'unix:{qmp_path},server=on,wait=off', '-display', 'none', '-monitor', 'none', '-no-reboot']
                output = bytearray()
                consumed = 0
                serial = qmp = None
                with (out / f'qemu-{boot}.log').open('wb') as log:
                    vm = subprocess.Popen(cmd, cwd=ROOT, stdout=log, stderr=log)
                    try:
                        serial = connect_serial(port, 20)
                        serial.settimeout(.2)
                        qmp = QMP(qmp_path)
                        def wait(marker, timeout=60):
                            nonlocal consumed
                            start = time.monotonic()
                            while marker not in output[consumed:]:
                                if not consumed:
                                    qmp.boot_if_menu_stalled(output, time.monotonic() - start)
                                if time.monotonic() - start > timeout or vm.poll() is not None:
                                    raise RuntimeError(f'boot {boot}: missing {marker!r}')
                                try:
                                    data = serial.recv(16384)
                                    if not data:
                                        raise RuntimeError('Serial disconnected')
                                    output.extend(data)
                                except socket.timeout:
                                    pass
                            consumed = output.index(marker, consumed) + len(marker)
                        wait(b'srv> ', 100)
                        send_serial_line(serial, 'run /fat/bin/sh\n')
                        wait(b' $ ')
                        script = 'workflow.sh' if boot == 1 else 'reopen.sh'
                        send_serial_line(serial, f'sh /fat/bc-test/{script}; echo __BC_DONE__\n')
                        wait(b'\r\n__BC_DONE__\r\n', 120)
                        send_serial_line(serial, 'exit\n')
                        wait(b'srv> ')
                        send_serial_line(serial, 'fsck /fat\n')
                        wait(b'exfat-check:')
                        wait(b'srv> ')
                        if not re.search(rb'exfat-check: mounts=1 files=\d+ clusters=\d+ leaks=0 fat=0 errors=0 ok', output):
                            raise RuntimeError('Guest exFAT check failed')
                        if has_fatal_exception(output.decode(errors='replace')):
                            raise RuntimeError('Guest reported a fatal exception')
                    finally:
                        if serial: serial.close()
                        if qmp: qmp.stream.close(); qmp.sock.close()
                        stop_process(vm)
                        (out / f'serial-{boot}.log').write_bytes(output)
                actual_image = ExfatFixture(disk.read_bytes())
                for name, expected in EXPECTED.items():
                    if boot == 1 and name.startswith('reopen'):
                        continue
                    actual = actual_image.read('bc-test/' + name)
                    if actual != expected:
                        raise RuntimeError(f'{name}: expected {expected!r}, got {actual!r}')
                version = actual_image.read('bc-test/version.txt')
                if not version.startswith(b'bc 7.0.3\n'):
                    raise RuntimeError(f'Unexpected bc version: {version!r}')
                for name in UPSTREAM:
                    actual = actual_image.read(f'bc-test/upstream-{name}-out.txt')
                    expected = payloads[f'upstream-{name}_results.txt']
                    if actual != expected:
                        raise RuntimeError(f'upstream {name}: output differs from unchanged fixture')
                # Upstream BcStatus: math=1, parse=2, fatal I/O=4.
                for name, expected_status in (('syntax', b'2'), ('divide', b'1'), ('missing', b'4')):
                    status = actual_image.read(f'bc-test/{name}-status.txt').strip()
                    if status != expected_status:
                        raise RuntimeError(f'{name}: expected status {expected_status!r}, got {status!r}')
                    diagnostic = actual_image.read(f'bc-test/{name}-error.txt')
                    if not diagnostic.strip():
                        raise RuntimeError(f'{name}: missing diagnostic')
                check = subprocess.run([str(driver), str(disk), 'allocation-check'], capture_output=True, text=True)
                (out / f'allocation-{boot}.log').write_text(check.stdout + check.stderr)
                if check.returncode or 'leaked=0 fat_bitmap_mismatches=0 errors=0' not in check.stdout:
                    raise RuntimeError('Allocation check failed')
                if args.fsck:
                    check = subprocess.run([args.fsck, '-n', str(disk)], capture_output=True, text=True)
                    (out / f'fsck-{boot}.log').write_text(check.stdout + check.stderr)
                    if check.returncode or 'ERROR' in check.stdout + check.stderr or 'corrupted' in check.stdout + check.stderr:
                        raise RuntimeError('External fsck failed')
        receipt.update(status='PASS', external_fsck='PASS' if args.fsck else 'SKIP: unavailable')
        save()
        print('bc smoke: PASS', flush=True)
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        save()
        raise


if __name__ == '__main__':
    main()
