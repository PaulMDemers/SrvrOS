#!/usr/bin/env python3
"""Expert walkthrough of the bounded xz workflow in a disposable Terminal VM."""
import hashlib
import json
import lzma
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
REPORT = b'XZ report\nAlpha 125\nBeta 275\nTotal 400\n'


def main():
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=ROOT / 'build/review/xz-terminal'))
    fixture = ExfatFixture((ROOT / 'build/srvros.exfat').read_bytes())
    fixture.add_directory_files('xz-ui', {
        'report.txt': REPORT, 'host.xz': lzma.compress(REPORT, preset=6),
        'bad.xz': b'not an xz stream\n'})
    disk = out / 'disk.exfat'
    disk.write_bytes(fixture.data)
    receipt = {'status': 'INCOMPLETE', 'scope': 'actual Terminal functional/expert walkthrough; no user study',
               'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'lessons': 'only successful exit plus exact readback accepts output; failed redirected output may be partial'}
    output = bytearray()
    consumed = 0
    with tempfile.TemporaryDirectory(prefix='xz-ui-') as temp:
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
                    type_text(f'echo $? > /fat/xz-ui/{name}.status\n')
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
                command('xz -0 -c /fat/xz-ui/report.txt > /fat/xz-ui/report.xz', 'compress')
                command('xz -t /fat/xz-ui/report.xz', 'test')
                command('xz -dc /fat/xz-ui/report.xz > /fat/xz-ui/restored.txt', 'restore')
                command('cmp /fat/xz-ui/report.txt /fat/xz-ui/restored.txt', 'compare')
                command('xz -dc /fat/xz-ui/host.xz > /fat/xz-ui/host.txt', 'host')
                command('xz -t /fat/xz-ui/bad.xz', 'bad')
                shot('01-error-recovery')
                command('cmp /fat/xz-ui/report.txt /fat/xz-ui/host.txt', 'host-compare')
                for cycle in range(2):
                    type_text(f'sleep 30 | xz -0 -c > /fat/xz-ui/interrupted-{cycle}.xz\n')
                    time.sleep(1)
                    qmp.call('human-monitor-command', {'command-line': 'sendkey ctrl-c'})
                    time.sleep(1)
                    type_text(f'echo $? > /fat/xz-ui/signal-{cycle}.status\n')
                    time.sleep(.5)
                type_text('cat /fat/xz-ui/restored.txt\n')
                time.sleep(.8)
                type_text('echo XZ-terminal-complete > /fat/xz-ui/complete.txt\n')
                time.sleep(.5)
                shot('02-restored-report')
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
    for name in ('compress', 'test', 'restore', 'compare', 'host', 'host-compare'):
        assert actual.read('xz-ui/' + name + '.status') == b'0\n', name
    assert actual.read('xz-ui/bad.status').strip() in (b'1', b'2')
    for cycle in range(2):
        assert actual.read(f'xz-ui/signal-{cycle}.status').strip() == b'130'
    assert actual.read('xz-ui/complete.txt') == b'XZ-terminal-complete\n'
    for name in ('report.txt', 'restored.txt', 'host.txt'):
        assert actual.read('xz-ui/' + name) == REPORT, name
    assert lzma.decompress(actual.read('xz-ui/report.xz')) == REPORT
    assert b'exception:' not in output and b'deskd: FAIL' not in output
    receipt.update(status='PASS', screenshots='expert visual inspection required',
                   runtime_sha256=hashlib.sha256(actual.read('bin/xz')).hexdigest())
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('xz actual Terminal: PASS', out, flush=True)


if __name__ == '__main__':
    (ROOT / 'build/review/xz-terminal').mkdir(parents=True, exist_ok=True)
    main()
