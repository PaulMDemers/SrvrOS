#!/usr/bin/env python3
"""Exercise Q35 level SCI assertion, deferred clear, shared dispatch and removal."""
import os
from pathlib import Path
from serial_records import search_complete
import subprocess
import tempfile
import time
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process


def main():
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/review/acpi-sci'
    out.mkdir(parents=True, exist_ok=True)
    output = bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-sci-') as directory:
        tmp = Path(directory)
        iso = boot.build_iso(str(root), str(tmp), 1280, 800, os.environ.copy())
        port = boot.choose_port(30001, 38000)
        command = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', iso,
                   '-boot', 'd', '-nic', 'none', '-serial',
                   f'tcp:127.0.0.1:{port},server=on,wait=off', '-qmp',
                   f'unix:{tmp}/qmp,server=on,wait=off', '-display', 'none',
                   '-monitor', 'none', '-no-reboot']
        with (out / 'qemu.txt').open('w') as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                serial = boot.connect_serial(port, 20)
                serial.settimeout(.2)
                qmp = QMP(tmp / 'qmp')
                start = time.monotonic()
                while b'srv> ' not in output and time.monotonic() - start < 60:
                    output.extend(boot.read_for(serial, .5))
                    qmp.boot_if_menu_stalled(output, time.monotonic() - start)
                assert b'srv> ' in output

                def request(text, pattern):
                    serial.sendall(text.encode() + b'\n')
                    part = bytearray()
                    until = time.monotonic() + 4
                    match = None
                    while not match and time.monotonic() < until:
                        part.extend(boot.read_for(serial, .1))
                        match = search_complete(pattern, part)
                    output.extend(part)
                    assert match, bytes(part)
                    return match

                def status(active, count):
                    match = request('scitest status', rb'sci-test: active=(\d+) interrupts=(\d+) shared=(\d+) events=(\d+) failures=(\d+)')
                    values = tuple(map(int, match.groups()))
                    assert values == (active, count * 2, count * 2, count, 0), values

                for _ in range(3):
                    request('scitest arm-fail', rb'sci-test: arm failed cleanup=yes\r?\nsci-test: REFUSED')
                    status(0, 0)
                    request('scitest arm', rb'sci-test: ARMED gsi=9 vector=41')
                    status(1, 0)
                    for count in range(1, 9):
                        qmp.call('system_powerdown')
                        output.extend(boot.read_for(serial, .1))
                        status(1, count)
                    request('scitest stop', rb'sci-test: stopped=yes')
                    qmp.call('system_powerdown')
                    output.extend(boot.read_for(serial, .1))
                    status(0, 8)
                    assert qmp.call('query-status')['running']
                qmp.call('quit')
                process.wait(timeout=10)
            finally:
                stop_process(process)
                (out / 'serial.txt').write_bytes(output)
    print('acpi-sci: PASS failed-start cleanup and three SCI lifetimes; 24 power events each delivered twice while level asserted, deferred source clear, shared handler, restored masked route and no post-stop callbacks')


if __name__ == '__main__':
    main()
