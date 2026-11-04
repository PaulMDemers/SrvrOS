#!/usr/bin/env python3
"""Exercise less through the actual graphical Terminal on a disposable VM."""
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


def main():
    out = ROOT / 'build/review/less-terminal'
    out.mkdir(parents=True, exist_ok=True)
    image = ExfatFixture((ROOT / 'build/srvros.exfat').read_bytes())
    report = ''.join(f'REPORT {i:03d}' + (' NEEDLE' if i in (120, 180) else '') + '\n' for i in range(1, 241))
    image.add_directory_files('less-ui', {'report.txt': report.encode()})
    disk = out / 'disk.exfat'
    disk.write_bytes(image.data)
    output = bytearray()
    consumed = 0
    receipt = {'status': 'INCOMPLETE', 'scope': 'actual Terminal expert walkthrough, no user study'}
    with tempfile.TemporaryDirectory(prefix='less-ui-') as temp:
        tmp = Path(temp)
        iso = build_iso(str(ROOT), str(tmp), 1280, 800, os.environ.copy())
        port = choose_port(30001, 38000)
        command = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', iso, '-boot', 'd',
                   '-drive', f'if=none,id=disk,file={disk},format=raw', '-device', 'ich9-ahci,id=ahci',
                   '-device', 'ide-hd,drive=disk,bus=ahci.0', '-device', 'qemu-xhci,id=xhci',
                   '-device', 'usb-kbd,bus=xhci.0', '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off',
                   '-qmp', f'unix:{tmp / "qmp"},server=on,wait=off', '-display', 'none', '-monitor', 'none', '-no-reboot']
        serial = qmp = None
        with (out / 'qemu.log').open('wb') as log:
            vm = subprocess.Popen(command, stdout=log, stderr=log)
            try:
                serial = connect_serial(port, 30)
                serial.settimeout(.2)
                qmp = QMP(tmp / 'qmp')

                def wait(marker, timeout=60):
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
                           '>': 'shift-dot', '|': 'shift-backslash', ';': 'semicolon'}

                def type_text(text):
                    for char in text:
                        key = mapping.get(char, char.lower())
                        shifted = char.isupper() or key.startswith('shift-')
                        qmp.key(key.removeprefix('shift-'), shifted)

                def shot(name):
                    time.sleep(.6)
                    qmp.call('screendump', {'filename': str(out / f'{name}.ppm')})

                pointer = [0, 0]

                def move(x, y):
                    qmp.mouse(x - pointer[0], y - pointer[1])
                    pointer[:] = [x, y]

                def button(down):
                    qmp.mouse(0, 0, 1 if down else 0)

                def ctrl_c():
                    qmp.call('human-monitor-command', {'command-line': 'sendkey ctrl-c'})
                    time.sleep(.8)

                wait(b'srv> ', 100)
                serial.sendall(b'run /fat/bin/deskd --apps-trace\n')
                wait(b'desktools: starting role=text')
                wait(b'deskd: commit')
                # Saturate at the corner before tracking relative PS/2 motion.
                qmp.mouse(-1600, -1000)
                move(890, 750)
                button(True)
                button(False)
                wait(b'deskterm: ready')
                time.sleep(1)
                type_text('stty > /fat/less-ui/before.txt\n')
                time.sleep(.5)
                type_text('less /fat/less-ui/report.txt\n')
                wait(b'/fat/bin/less entry=')
                shot('01-open')
                qmp.key('spc')
                shot('02-page-forward')
                type_text('b')
                shot('03-page-back')
                type_text('G')
                shot('04-end')
                type_text('g/NEEDLE\n')
                shot('05-search')
                type_text('n')
                shot('06-search-next')
                move(882, 636)
                button(True)
                move(1056, 681)
                button(False)
                shot('07-resized')
                ctrl_c()
                type_text('q')
                time.sleep(1)
                type_text('stty > /fat/less-ui/after.txt\n')
                type_text('cat /fat/less-ui/report.txt | less\n')
                time.sleep(2)
                shot('08-pipeline')
                type_text('G')
                shot('09-pipeline-end')
                type_text('q')
                time.sleep(1)
                for cycle in range(2):
                    type_text('sleep 8 | less\n')
                    time.sleep(2)
                    time.sleep(.7)
                    ctrl_c()
                    type_text('q')
                    time.sleep(1)
                    type_text(f'echo survived-{cycle} > /fat/less-ui/signal-{cycle}.txt\n')
                type_text('echo less-ui-ok > /fat/less-ui/complete.txt\n')
                shot('10-shell-restored')
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
    for name, expected in [('complete.txt', b'less-ui-ok\n'), ('signal-0.txt', b'survived-0\n'), ('signal-1.txt', b'survived-1\n')]:
        assert actual.read('less-ui/' + name) == expected, name
    before_text = actual.read('less-ui/before.txt')
    after_text = actual.read('less-ui/after.txt')
    assert before_text != after_text, 'Terminal resize did not change dimensions'
    before = before_text.split()[-3:]
    after = after_text.split()[-3:]
    assert before == after == [b'icanon', b'echo', b'icrnl'], (before, after)
    assert b'exception:' not in output and b'deskd: FAIL' not in output
    receipt.update(status='PASS', terminal_modes_restored=True, repeated_interruption_shell_continuation=True,
                   screenshots='expert visual inspection required; exact screen assertions are in less_smoke')
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('less actual Terminal: PASS; inspect screenshots', out, flush=True)


if __name__ == '__main__':
    main()
