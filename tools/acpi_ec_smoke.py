#!/usr/bin/env python3
"""Synthetic EC through real uACPI regions, query methods and SBS transport."""
import os
from pathlib import Path
from serial_records import search_complete, complete_lines
import subprocess
import tempfile
import time
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process


def main():
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/review/acpi-ec'
    out.mkdir(parents=True, exist_ok=True)
    output = bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-ec-') as directory:
        tmp = Path(directory)
        subprocess.run(['iasl', '-p', str(tmp/'worker'), str(root/'tools/fixtures/acpi/runtime-worker.asl')],
                       check=True, stdout=subprocess.DEVNULL)
        subprocess.run(['iasl', '-p', str(tmp/'ec'), str(root/'tools/fixtures/acpi/runtime-ec.asl')], check=True, stdout=subprocess.DEVNULL)
        iso = boot.build_iso(str(root), str(tmp), 1280, 800, os.environ.copy(), root/'build/acpi-runtime/srvros.elf')
        port = boot.choose_port(30001, 38000)
        command = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', iso,
                   '-boot', 'd', '-nic', 'none', '-acpitable', f'file={tmp}/worker.aml', '-acpitable', f'file={tmp}/ec.aml',
                   '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off', '-qmp',
                   f'unix:{tmp}/qmp,server=on,wait=off', '-display', 'none',
                   '-monitor', 'none', '-no-reboot']
        with (out / 'qemu.txt').open('w') as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                serial = boot.connect_serial(port, 20)
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

                def snapshot(queries, notes, transactions, generation=1, current=1, failed=0, poisoned=0):
                    until = time.monotonic() + 8
                    while time.monotonic() < until:
                        match = request('acpiruntime ec status', rb'acpi-ec: ready=(\d+) queries=(\d+) notify=(\d+) queued_in_sample=(\d+) errors=(\d+) foreign=(\d+) attached=(\d+) detached=(\d+) pending=(\d+)\r?\nacpi-ec: generation=(\d+) current=(\d+) soc=(\d+) voltage_uv=(\d+) transactions=(\d+) failed=(\d+) poisoned=(\d+)')
                        v = tuple(map(int, match.groups()))
                        assert v[4] == 0 and v[1] <= queries and v[2] <= notes, v
                        if v[0] == 1 and v[1:3] == (queries, notes) and v[8] == 0 and v[9] == generation and v[14] == failed:
                            assert v[6:8] == (1, 0), v
                            if generation:
                                assert v[3:6] == (2, 0, 1) and v[9:] == (generation, current, 86, 12000000, transactions, failed, poisoned), v
                            return
                    raise AssertionError(('EC did not complete', v))

                for _ in range(2):
                    request('acpiruntime start', rb'acpi-runtime: READY')
                    request('acpiruntime ec start', rb'acpi-ec: setup=queued')
                    snapshot(0, 0, 0, generation=0, current=0)
                    request('acpiruntime ec foreign', rb'acpi-ec: foreign=REFUSED')
                    request('acpiruntime ec sample', rb'acpi-ec: sample=queued')
                    snapshot(2, 1, 16)
                    qmp.call('system_powerdown')
                    snapshot(3, 2, 16)
                    request('acpiruntime ec queries', rb'acpi-ec: queries=queued')
                    snapshot(35, 18, 32)
                    request('acpiruntime ec poison', rb'acpi-ec: sample=queued')
                    snapshot(35, 18, 32, current=0, failed=1, poisoned=1)
                    request('acpiruntime ec sample', rb'acpi-ec: sample=REFUSED')
                    request('acpiruntime stop', rb'acpi-ec: retired=yes attached=1 detached=1\r?\nacpi-runtime: stopped=yes heap=0 errors=0 denied=0')
                request('acpiruntime start', rb'acpi-runtime: READY')
                request('acpiruntime ec start-slow', rb'acpi-ec: setup=queued')
                request('acpiruntime stop', rb'acpi-ec: retired=yes attached=1 detached=1\r?\nacpi-runtime: stopped=yes heap=0 errors=0 denied=0')
                qmp.call('quit')
                process.wait(timeout=10)
            finally:
                stop_process(process)
                (out / 'serial.txt').write_bytes(output)
    print('acpi-ec: PASS two lifetimes, _REG read/write, single-worker ownership, SCI-triggered queries/Notify, 32-query batches, deferred queries during full SBS samples, poison/last-good retention delayed-setup teardown and detached/leak-free reset')


if __name__ == '__main__':
    main()
