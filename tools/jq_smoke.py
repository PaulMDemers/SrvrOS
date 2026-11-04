#!/usr/bin/env python3
"""Exercise the staged jq and sqlite3 CLIs on a disposable copy of the guest disk.

Requires the checkout's built ISO, exFAT image and build/tests/exfat-driver.
No build or staging is performed. Evidence is saved under build/jq-smoke.
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
    'sales.json': b'{"sales":[{"category":"red","amount":3},{"category":"blue","amount":4},{"category":"red","amount":5},{"category":"blue","amount":6},{"category":"skip","amount":0}]}\n',
    'report.jq': b'[.sales[] | select(.amount > 0)] | sort_by(.category) | group_by(.category) | map({category: .[0].category, total: (map(.amount) | add)})\n',
    'csv.jq': b'.[] | [.category, .total] | @csv\n',
    'values.json': b'["caf\xc3\xa9","line\\nquote\\\"slash\\\\","nul\\u0000end"]\n',
    'values.jq': b'.[]\n',
    'numbers.json': b'[123456789012345678901234567890,1.2300,1e100]\n',
    'identity.jq': b'.\n',
    'true.jq': b'true\n',
    'false.jq': b'false\n',
    'empty.jq': b'empty\n',
    'bad.jq': b'.[\n',
    'regex.jq': b'match("a")\n',
    'regex.json': b'"a"\n',
    'bad.json': b'{"broken":\n',
    'import.sql': b'.bail on\nCREATE TABLE totals(category TEXT, total INTEGER);\n.mode csv\n.import report.csv totals\nSELECT * FROM totals ORDER BY category;\n',
    'reopen.sql': b'SELECT * FROM totals ORDER BY category;\nPRAGMA integrity_check;\n',
    'workflow.sh': b'''#!/fat/bin/sh
cd /fat/jq-test
jq --version > version.txt
echo $? > version-status.txt
chmod 755 /fat/jq-test/jq-output-test
/fat/jq-test/jq-output-test > output-probe.txt 2> output-probe-error.txt
echo $? > output-probe-status.txt
jq --run-tests /fat/jq-test/upstream-selected.test > upstream.txt 2> upstream-error.txt
echo $? > upstream-status.txt
jq -c -f report.jq sales.json > report.json 2> report-error.txt
echo $? > report-status.txt
jq -r -f csv.jq report.json > report.csv 2> csv-error.txt
echo $? > csv-status.txt
sqlite3 report.db < import.sql > imported.txt 2> import-error.txt
echo $? > import-status.txt
cat sales.json | jq -c -f report.jq > pipe.txt 2> pipe-error.txt
echo $? > pipe-status.txt
jq -r -f values.jq values.json > values.txt 2> values-error.txt
echo $? > values-status.txt
jq -c -f identity.jq numbers.json > numbers.txt 2> numbers-error.txt
echo $? > numbers-status.txt
jq -n -e -f true.jq > true.txt 2> true-error.txt
echo $? > true-status.txt
jq -n -e -f false.jq > false.txt 2> false-error.txt
echo $? > false-status.txt
jq -n -e -f empty.jq > empty.txt 2> empty-error.txt
echo $? > empty-status.txt
jq -f identity.jq bad.json > malformed.txt 2> malformed-error.txt
echo $? > malformed-status.txt
jq -n -f bad.jq > filter.txt 2> filter-error.txt
echo $? > filter-status.txt
jq -f identity.jq absent.json > missing.txt 2> missing-error.txt
echo $? > missing-status.txt
jq -f regex.jq regex.json > regex.txt 2> regex-error.txt
echo $? > regex-status.txt
jq -f identity.jq sales.json > /fat/jq-test/no-such-dir/output.txt
echo $? > output-status.txt
jq -c -f report.jq sales.json > recovery.txt 2> recovery-error.txt
echo $? > recovery-status.txt
''',
    'reopen.sh': b'''#!/fat/bin/sh
cd /fat/jq-test
sqlite3 report.db < reopen.sql > reopened.txt 2> reopen-error.txt
echo $? > reopen-status.txt
jq -c -f identity.jq report.json > persisted.txt 2> persisted-error.txt
echo $? > persisted-status.txt
''',
}
REPORT = b'[{"category":"blue","total":10},{"category":"red","total":8}]\n'
EXPECTED = {
    'version.txt': b'jq-1.8.2\n',
    'version-status.txt': b'0\n',
    'report.json': REPORT, 'pipe.txt': REPORT, 'recovery.txt': REPORT, 'persisted.txt': REPORT,
    'report.csv': b'"blue",10\n"red",8\n',
    'imported.txt': b'blue,10\r\nred,8\r\n',
    'reopened.txt': b'blue|10\nred|8\nok\n',
    'values.txt': b'caf\xc3\xa9\nline\nquote"slash\\\nnul\x00end\n',
    'numbers.txt': b'[123456789012345678901234567890,1.2300,1E+100]\n',
    'true.txt': b'true\n', 'false.txt': b'false\n', 'empty.txt': b'',
    'output-probe.txt': b'jq-output-error: PASS\n',
    'output-probe-status.txt': b'0\n',
    'upstream-status.txt': b'0\n',
    'true-status.txt': b'0\n', 'false-status.txt': b'1\n', 'empty-status.txt': b'4\n',
    **{name + '-status.txt': b'0\n' for name in ('report', 'csv', 'import', 'pipe', 'values', 'numbers', 'recovery', 'reopen', 'persisted')},
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
        staged = image.read('bin/jq')
        image.read('bin/sqlite3')
    except (StopIteration, ValueError) as error:
        raise RuntimeError('Source disk must contain /fat/bin/jq and /fat/bin/sqlite3') from error
    out_root = ROOT / 'build/jq-smoke'
    out_root.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'), dir=out_root))
    disk = out / 'disk.exfat'
    payloads = dict(FILES)
    payloads['jq-output-test'] = runtime_elf((ROOT / 'build/userspace/jq-output-test.elf').read_bytes())
    # The first eleven unchanged parser/value cases, ending before the first
    # %%FAIL case. This is a bounded selection, not the full upstream suite.
    upstream = (ROOT / 'ports/upstream/jq/tests/jq.test').read_bytes()
    selected, marker, _ = upstream.partition(b'%%FAIL')
    if not marker or b'\ntrue\nnull\ntrue\n' not in selected:
        raise RuntimeError('Unexpected upstream jq.test fixture')
    payloads['upstream-selected.test'] = selected
    image.add_directory_files('jq-test', payloads)
    disk.write_bytes(image.data)
    receipt = {'status': 'INCOMPLETE', 'iso_sha256': sha(iso), 'source_disk_sha256': sha(source),
               'staged_jq_sha256': hashlib.sha256(staged).hexdigest(),
               'selected_upstream_sha256': hashlib.sha256(selected).hexdigest(),
               'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'source_status': subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
               'scope': 'jq JSON report, SQLite import and reboot persistence; no regex suite'}
    receipt_path = out / 'receipt.json'
    def save():
        receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    save()
    print('jq smoke evidence:', out, flush=True)
    try:
        with tempfile.TemporaryDirectory(prefix='jq-vm-') as temp:
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
                        send_serial_line(serial, f'sh /fat/jq-test/{script}; echo __JQ_DONE__\n')
                        wait(b'\r\n__JQ_DONE__\r\n', 120)
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
                    if boot == 1 and (name.startswith('reopen') or name.startswith('persisted')):
                        continue
                    actual = actual_image.read('jq-test/' + name)
                    if actual != expected:
                        raise RuntimeError(f'{name}: expected {expected!r}, got {actual!r}')
                upstream_output = actual_image.read('jq-test/upstream.txt')
                if b'11 of 11 tests passed (0 malformed, 0 skipped)' not in upstream_output:
                    raise RuntimeError(f'Selected upstream tests failed: {upstream_output!r}')
                for name in ('malformed', 'filter', 'missing', 'regex', 'output'):
                    status = actual_image.read('jq-test/' + name + '-status.txt').strip()
                    if not status.isdigit() or int(status) == 0:
                        raise RuntimeError(f'{name} unexpectedly succeeded: {status!r}')
                for name in ('malformed', 'filter', 'missing'):
                    diagnostic = actual_image.read('jq-test/' + name + '-error.txt')
                    if not diagnostic.strip():
                        raise RuntimeError(f'{name}: missing diagnostic')
                regex_error = actual_image.read('jq-test/regex-error.txt')
                if b'compiled without ONIGURUMA regex library' not in regex_error:
                    raise RuntimeError(f'regex: expected unsupported diagnostic, got {regex_error!r}')
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
        print('jq smoke: PASS', flush=True)
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        save()
        raise


if __name__ == '__main__':
    main()
