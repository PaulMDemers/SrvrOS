#!/usr/bin/env python3
"""Exercise the staged sqlite3 CLI on a disposable copy of the guest disk.

Requires the checkout's built ISO, exFAT image and build/tests/exfat-driver.
No build or staging is performed. Evidence is saved under build/sqlite-cli-smoke.
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
FILES = {
    'input.csv': b'category,amount\nred,3\nblue,4\nred,5\n',
    'setup.sql': b'''.bail on
.mode csv
.import input.csv sales
BEGIN;
INSERT INTO sales VALUES('blue',6);
COMMIT;
BEGIN;
INSERT INTO sales VALUES('discard',99);
ROLLBACK;
.read report.sql
.once export.csv
SELECT category, sum(amount) FROM sales GROUP BY category ORDER BY category;
''',
    'report.sql': b'SELECT category, sum(amount) FROM sales GROUP BY category ORDER BY category;\n',
    'integrity.sql': b'PRAGMA integrity_check;\nSELECT count(*) FROM sales;\n',
    'invalid.sql': b'.bail on\nSELECT FROM invalid;\n',
    'readonly.sql': b'.bail on\nINSERT INTO sales VALUES(\'nope\',1);\n',
    'attach.sql': b".bail on\nATTACH 'other.db' AS other;\n",
    'journal.sql': b'.bail on\nPRAGMA journal_mode=OFF;\n',
    'sync.sql': b'.bail on\nPRAGMA synchronous=OFF;\n',
    'write-control.sql': b".bail on\nSELECT writefile('control.txt','hello');\n",
    'write-alias.sql': b".bail on\nSELECT writefile('./test.db','target',41471);\n",
    'workflow.sh': b'''#!/fat/bin/sh
chmod 755 /fat/sqlite-test/sqlite-vfs-test
/fat/sqlite-test/sqlite-vfs-test > /fat/sqlite-test/vfs.txt
echo $? > /fat/sqlite-test/vfs-status.txt
cd /fat/sqlite-test
sqlite3 test.db < setup.sql > report.txt
echo $? > setup-status.txt
sqlite3 test.db < integrity.sql > integrity.txt
echo $? > integrity-status.txt
sqlite3 test.db < invalid.sql > invalid.txt
echo $? > invalid-status.txt
sqlite3 -readonly test.db < integrity.sql > readonly-read.txt
echo $? > readonly-read-status.txt
sqlite3 -readonly test.db < readonly.sql > readonly.txt
echo $? > readonly-status.txt
sqlite3 test.db < attach.sql > attach.txt 2> attach-error.txt
echo $? > attach-status.txt
sqlite3 test.db < journal.sql > journal.txt 2> journal-error.txt
echo $? > journal-status.txt
sqlite3 test.db < sync.sql > sync.txt 2> sync-error.txt
echo $? > sync-status.txt
sqlite3 test.db < write-control.sql > write-control.txt
echo $? > write-control-status.txt
sqlite3 test.db < write-alias.sql > write-alias.txt
echo $? > write-alias-status.txt
sqlite3 test.db < integrity.sql > after-errors.txt
echo $? > after-status.txt
''',
    'reopen.sh': b'''#!/fat/bin/sh
cd /fat/sqlite-test
sqlite3 test.db < integrity.sql > reboot.txt
echo $? > reboot-status.txt
''',
}
EXPECTED = {
    'vfs.txt': b'sqlite-vfs-guest: PASS\n',
    'vfs-status.txt': b'0\n',
    'report.txt': b'blue,10\r\nred,8\r\n',
    'export.csv': b'blue,10\r\nred,8\r\n',
    'integrity.txt': b'ok\n4\n',
    'after-errors.txt': b'ok\n4\n',
    'readonly-read.txt': b'ok\n4\n',
    'write-control.txt': b'5\n',
    'control.txt': b'hello',
    'reboot.txt': b'ok\n4\n',
    **{name + '-status.txt': b'0\n' for name in ('setup', 'integrity', 'after', 'reboot', 'readonly-read', 'write-control')},
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
        staged = image.read('bin/sqlite3')
    except (StopIteration, ValueError) as error:
        raise RuntimeError('Source disk does not contain /fat/bin/sqlite3') from error
    out_root = ROOT / 'build/sqlite-cli-smoke'
    out_root.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'), dir=out_root))
    disk = out / 'disk.exfat'
    payloads = dict(FILES)
    payloads['sqlite-vfs-test'] = runtime_elf((ROOT / 'build/userspace/sqlite-vfs-test.elf').read_bytes())
    image.add_directory_files('sqlite-test', payloads)
    disk.write_bytes(image.data)
    receipt = {'status': 'INCOMPLETE', 'iso_sha256': sha(iso), 'source_disk_sha256': sha(source),
               'staged_sqlite3_sha256': hashlib.sha256(staged).hexdigest(),
               'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'source_status': subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
               'scope': 'CLI workflow, reboot persistence and VFS exclusive ownership probe'}
    receipt_path = out / 'receipt.json'
    def save():
        receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    save()
    print('sqlite CLI evidence:', out, flush=True)
    try:
        with tempfile.TemporaryDirectory(prefix='sqlite-cli-vm-') as temp:
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
                        send_serial_line(serial, f'sh /fat/sqlite-test/{script}; echo __SQLITE_DONE__\n')
                        wait(b'\r\n__SQLITE_DONE__\r\n', 120)
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
                    if boot == 1 and name.startswith('reboot'):
                        continue
                    actual = actual_image.read('sqlite-test/' + name)
                    if actual != expected:
                        raise RuntimeError(f'{name}: expected {expected!r}, got {actual!r}')
                for name in ('invalid', 'readonly', 'attach', 'journal', 'sync', 'write-alias'):
                    status = actual_image.read('sqlite-test/' + name + '-status.txt').strip()
                    if not status.isdigit() or int(status) == 0:
                        raise RuntimeError(f'{name} unexpectedly succeeded: {status!r}')
                for name in ('attach', 'journal', 'sync'):
                    diagnostic = actual_image.read('sqlite-test/' + name + '-error.txt')
                    if b'not authorized' not in diagnostic.lower():
                        raise RuntimeError(f'{name}: expected authorization error, got {diagnostic!r}')
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
        print('sqlite CLI smoke: PASS (VFS ownership contention tested)', flush=True)
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        save()
        raise


if __name__ == '__main__':
    main()
