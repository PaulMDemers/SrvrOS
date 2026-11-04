#!/usr/bin/env python3
"""Expert walkthrough of the bounded GNU diff workflow in a disposable Terminal VM."""
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from exfat_test_image import ExfatFixture

ROOT = Path(__file__).resolve().parents[1]
BEFORE = b'alpha\nbeta\ngamma\n'
AFTER = b'alpha\nBETA\ninserted\ngamma\n'
EXPECTED = b'--- before\n+++ after\n@@ -1,3 +1,4 @@\n alpha\n-beta\n+BETA\n+inserted\n gamma\n'


def main():
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=ROOT / 'build/review/diff-terminal'))
    fixture = ExfatFixture((ROOT / 'build/srvros.exfat').read_bytes())
    fixture.add_directory_files('diff-ui', {'before.txt': BEFORE, 'after.txt': AFTER, 'target.txt': BEFORE})
    disk = out / 'disk.exfat'
    disk.write_bytes(fixture.data)
    receipt = {'status': 'INCOMPLETE', 'scope': 'actual Terminal functional/expert walkthrough; no user study',
               'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'lessons': 'Completion requires the expected status 0/1 and exact report/readback; status 2 is trouble; local patch only on this qualified subset'}
    output = bytearray()
    consumed = 0
    with tempfile.TemporaryDirectory(prefix='diff-ui-') as temp:
        tmp = Path(temp)
        iso = build_iso(str(ROOT), str(tmp), 1280, 800, os.environ.copy())
        port = choose_port(30001, 38000)
        cmd = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', iso, '-boot', 'd',
               '-drive', f'if=none,id=disk,file={disk},format=raw', '-device', 'ich9-ahci,id=ahci',
               '-device', 'ide-hd,drive=disk,bus=ahci.0', '-device', 'qemu-xhci,id=xhci',
               '-device', 'usb-kbd,bus=xhci.0', '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off',
               '-qmp', f'unix:{tmp / "qmp"},server=on,wait=off', '-display', 'none', '-monitor', 'none', '-no-reboot']
        serial = qmp = None
        with (out / 'qemu.log').open('wb') as log:
            vm = subprocess.Popen(cmd, stdout=log, stderr=log)
            try:
                serial = connect_serial(port, 30)
                serial.settimeout(.2)
                qmp = QMP(tmp / 'qmp')

                def wait(marker, timeout=90):
                    nonlocal consumed
                    start = time.monotonic()
                    while marker not in output[consumed:]:
                        if marker == b'srv> ' and not consumed:
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

                mapping = {' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus', '\n': 'ret',
                           '>': 'shift-dot', '|': 'shift-backslash', '?': 'shift-slash', '$': 'shift-4'}

                def type_text(text):
                    for char in text:
                        key = mapping.get(char, char.lower())
                        qmp.key(key.removeprefix('shift-'), char.isupper() or key.startswith('shift-'))

                def command(text, name):
                    type_text(text + '\n')
                    time.sleep(1)
                    type_text(f'echo $? > /fat/diff-ui/{name}.status\n')
                    time.sleep(.5)

                def shot(name):
                    qmp.call('screendump', {'filename': str(out / (name + '.ppm'))})

                wait(b'srv> ')
                serial.sendall(b'run /fat/bin/deskd --apps-trace\n')
                wait(b'desktools: starting role=text')
                wait(b'deskd: commit')
                qmp.mouse(-1600, -1000)
                qmp.mouse(890, 750)
                qmp.mouse(0, 0, 1)
                qmp.mouse(0, 0, 0)
                wait(b'deskterm: ready')
                time.sleep(1)
                command('diff -u -L before -L after /fat/diff-ui/before.txt /fat/diff-ui/after.txt > /fat/diff-ui/changes.diff', 'changed')
                type_text('less /fat/diff-ui/changes.diff\n')
                wait(b'/fat/bin/less entry=')
                time.sleep(.6)
                shot('01-unified-report')
                type_text('q')
                time.sleep(.6)
                command('patch -i /fat/diff-ui/changes.diff /fat/diff-ui/target.txt', 'patch')
                command('cmp /fat/diff-ui/target.txt /fat/diff-ui/after.txt', 'compare')
                command('diff -q /fat/diff-ui/target.txt /fat/diff-ui/after.txt', 'same')
                command('diff /fat/diff-ui/missing.txt /fat/diff-ui/after.txt', 'missing')
                shot('02-error-feedback')
                type_text('cat /fat/diff-ui/target.txt\n')
                time.sleep(.8)
                type_text('echo DIFF-terminal-complete > /fat/diff-ui/complete.txt\n')
                time.sleep(.5)
                shot('03-restored-target')
                type_text('exit\n')
                wait(b'deskterm: shell exited status=0')
                qmp.call('human-monitor-command', {'command-line': 'sendkey ctrl-shift-w'})
                wait(b'deskterm: exited failure=0')
                qmp.call('human-monitor-command', {'command-line': 'sendkey f10'})
                time.sleep(.3)
                qmp.key('end')
                qmp.key('ret')
                wait(b'deskd: exited')
                wait(b'srv> ')
            except Exception as error:
                receipt['error'] = str(error)
                if qmp:
                    qmp.call('screendump', {'filename': str(out / 'failure.ppm')})
                raise
            finally:
                if serial:
                    serial.close()
                if qmp:
                    qmp.stream.close()
                    qmp.sock.close()
                stop_process(vm)
                (out / 'serial.log').write_bytes(output)
                (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    actual = ExfatFixture(disk.read_bytes())
    for name in ('patch', 'compare', 'same'):
        assert actual.read('diff-ui/' + name + '.status') == b'0\n', name
    assert actual.read('diff-ui/changed.status') == b'1\n'
    assert actual.read('diff-ui/missing.status') == b'2\n'
    assert actual.read('diff-ui/complete.txt') == b'DIFF-terminal-complete\n'
    assert actual.read('diff-ui/before.txt') == BEFORE
    assert actual.read('diff-ui/after.txt') == AFTER
    assert actual.read('diff-ui/target.txt') == AFTER
    assert actual.read('diff-ui/changes.diff') == EXPECTED
    assert b'exception:' not in output and b'deskd: FAIL' not in output
    receipt.update(status='PASS', screenshots='expert visual inspection required',
                   runtime_sha256=hashlib.sha256(actual.read('bin/diff')).hexdigest())
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('GNU diff actual Terminal: PASS', out, flush=True)


if __name__ == '__main__':
    (ROOT / 'build/review/diff-terminal').mkdir(parents=True, exist_ok=True)
    main()
