#!/usr/bin/env python3
"""Full uACPI VM adapter: real SCI, deferred AML waits, rollback and reset."""
import os
from pathlib import Path
from vm_sockets import connect_unix
from serial_records import search_complete, complete_lines
import subprocess
import tempfile
import time
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process


def main():
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/review/acpi-runtime'
    out.mkdir(parents=True, exist_ok=True)
    output = bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-runtime-') as directory:
        tmp = Path(directory)
        subprocess.run(['iasl', '-p', str(tmp/'worker'), str(root/'tools/fixtures/acpi/runtime-worker.asl')],
                       check=True, stdout=subprocess.DEVNULL)
        iso = boot.build_iso(str(root), str(tmp), 1280, 800, os.environ.copy(), root/'build/acpi-runtime/srvros.elf')

        command = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', iso,
                   '-boot', 'd', '-nic', 'none', '-acpitable', f'file={tmp}/worker.aml',
                   '-serial', f'unix:{tmp}/serial,server=on,wait=off', '-qmp',
                   f'unix:{tmp}/qmp,server=on,wait=off', '-display', 'none',
                   '-monitor', 'none', '-no-reboot']
        with (out / 'qemu.txt').open('w') as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                serial = connect_unix(tmp / 'serial', 20)
                serial.settimeout(.05)
                qmp = QMP(tmp / 'qmp')
                start = time.monotonic()
                while b'srv> ' not in output and time.monotonic() - start < 60:
                    output.extend(boot.read_for(serial, .2))
                    qmp.boot_if_menu_stalled(output, time.monotonic() - start)
                assert b'srv> ' in output

                def request(text, pattern, timeout=8):
                    serial.sendall(text.encode() + b'\n')
                    part = bytearray()
                    until = time.monotonic() + timeout
                    match = None
                    while not match and time.monotonic() < until:
                        part.extend(boot.read_for(serial, .02))
                        match = search_complete(pattern, part)
                        if b'HALTED' in complete_lines(part) or b'panic' in complete_lines(part):
                            break
                    output.extend(part)
                    (out / 'serial.txt').write_bytes(output)
                    assert match, bytes(part)
                    return match

                def status(count):
                    until = time.monotonic() + 3
                    while time.monotonic() < until:
                        match = request('acpiruntime status', rb'acpi-runtime: active=(\d+) irq=(\d+) events=(\d+) work=(\d+) value=(\d+) errors=(\d+) denied=(\d+) heap=(\d+) peak=(\d+)')
                        values = tuple(map(int, match.groups()))
                        assert values[0] == 1 and values[5:7] == (0, 0), values
                        if values[3] == count:
                            assert values[2] == count and values[4] == count, values
                            assert count <= values[1] <= count + 2 and 0 < values[7] <= values[8]
                            return
                    raise AssertionError(('work did not complete', values))

                request('acpiruntime start-fixture', rb'acpi-runtime: REFUSED')
                request('acpiruntime start-early', rb'acpi-runtime: REFUSED')
                request('acpiruntime start-io-fail', rb'acpi-runtime: initialization failed status=the requested action has been denied cleanup=yes\r?\nacpi-runtime: REFUSED')
                assert b'initialize API=success refused errors=0 denied=1' in complete_lines(output)
                request('acpiruntime status', rb'acpi-runtime: active=0 irq=0 events=0 work=0 value=0 errors=0 denied=1 heap=0')
                for _ in range(3):
                    request('acpiruntime start-fail', rb'acpi-runtime: initialization failed status=the requested action has been denied cleanup=yes\r?\nacpi-runtime: REFUSED')
                    request('acpiruntime start', rb'acpi-runtime: READY')
                    status(0)
                    for count in range(1, 9):
                        qmp.call('system_powerdown')
                        status(count)
                    qmp.call('system_powerdown')
                    race = request('acpiruntime race', rb'acpi-runtime: race=PASS value=10 waits=(\d+) elapsed_us=(\d+)')
                    assert int(race[1]) > 0 and int(race[2]) >= 100000
                    status(9)
                    qmp.call('system_powerdown')
                    request('acpiruntime stop', rb'acpi-runtime: stopped=yes heap=0 errors=0 denied=0 pending=1')
                    qmp.call('system_powerdown')
                    match = request('acpiruntime status', rb'acpi-runtime: active=0 irq=(\d+) events=10 work=10 value=11 errors=0 denied=0 heap=0')
                    assert int(match[1]) >= 10
                    assert qmp.call('query-status')['running']
                request('acpiruntime start', rb'acpi-runtime: READY')
                request('acpiruntime wedge', rb'acpi-runtime: wedge-blocked')
                halted = request('acpiruntime stop', rb'acpi-runtime: HALTED unsafe teardown: work did not drain heap=(\d+)', timeout=8)
                assert int(halted[1]) > 0  # Live state was retained, not freed under blocked work.
                qmp.call('quit')
                process.wait(timeout=10)
            finally:
                stop_process(process)
                (out / 'serial.txt').write_bytes(output)
    print('acpi-runtime: PASS full event-enabled uACPI, failed-install rollback, three SCI/worker lifetimes, 30 deferred AML mutex/sleep evaluations, contended AML locks, pending-work teardown, zero errors/denials/leaks no callbacks after reset and failed-drain halt retaining live state')


if __name__ == '__main__':
    main()
