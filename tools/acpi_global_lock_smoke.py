#!/usr/bin/env python3
"""Q35-only full uACPI FACS lock lifecycle and fault containment."""
import argparse
import os
import re
from pathlib import Path
import subprocess
import tempfile
import time

import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from serial_records import search_complete, complete_lines
from vm_sockets import connect_unix


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fault', choices=['none', 'block', 'release-denied', 'handoff', 'deadline'], default='none')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/review' / ('acpi-global-lock-' + args.fault)
    out.mkdir(parents=True, exist_ok=True)
    output = bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-global-lock-') as directory:
        tmp = Path(directory)
        iso = boot.build_iso(str(root), str(tmp), 1280, 800, os.environ.copy(),
                             root/'build/acpi-runtime/srvros.elf')
        command = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', iso,
                   '-boot', 'd', '-nic', 'none', '-serial', f'unix:{tmp}/serial,server=on,wait=off',
                   '-qmp', f'unix:{tmp}/qmp,server=on,wait=off', '-display', 'none',
                   '-monitor', 'none', '-no-reboot']
        with (out/'qemu.txt').open('w') as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                serial = connect_unix(tmp/'serial', 20)
                serial.settimeout(.05)
                qmp = QMP(tmp/'qmp')
                started = time.monotonic()
                while b'srv> ' not in output and time.monotonic()-started < 60:
                    output.extend(boot.read_for(serial, .2))
                    qmp.boot_if_menu_stalled(output, time.monotonic()-started)
                assert b'srv> ' in output

                def request(command, pattern, timeout=8):
                    serial.sendall(command.encode()+b'\n')
                    part = bytearray()
                    end = time.monotonic()+timeout
                    match = None
                    while not match and time.monotonic() < end:
                        part.extend(boot.read_for(serial, .02))
                        match = search_complete(pattern, part)
                        if b'HALTED' in complete_lines(part) or b'panic' in complete_lines(part):
                            break
                    output.extend(part)
                    (out/'serial.txt').write_bytes(output)
                    assert match, bytes(part)
                    return match

                for _ in range(2 if args.fault in ('none','handoff') else 1):
                    request('acpiruntime start', rb'acpi-runtime: READY')
                    if args.fault in ('none','handoff'):
                        request('acpiruntime global-lock-handoff' if args.fault=='handoff' else 'acpiruntime global-lock', rb'acpi-global-lock: PASS')
                        request('acpiruntime stop', rb'acpi-runtime: stopped=yes heap=0 errors=0 denied=0 pending=\d+')
                    elif args.fault == 'block':
                        request('acpiruntime global-lock-block', rb'acpi-global-lock: firmware-wait')
                        halted = request('acpiruntime stop', rb'acpi-runtime: HALTED unsafe teardown: pre-reset work did not drain heap=(\d+)')
                        assert int(halted[1]) > 0
                        retained = search_complete(rb'acpi-global-lock: retained lock=3 sci_phase=1 token=(\d+) heap=(\d+)', output)
                        assert retained and int(retained[1]) and int(retained[2]) == int(halted[1])
                    elif args.fault == 'deadline':
                        halted = request('acpiruntime global-lock-deadline', rb'acpi-runtime: HALTED unsafe teardown: Global Lock operation deadline; state retained heap=(\d+)')
                        assert int(halted[1]) > 0
                        retained = search_complete(rb'acpi-global-lock: deadline lock=3 sci_phase=1 token=(\d+) active=1', output)
                        assert retained and int(retained[1])
                        assert b'acpiruntime stop' not in output
                    else:
                        halted = request('acpiruntime global-lock-release-denied', rb'acpi-runtime: HALTED unsafe teardown: Global Lock release denied heap=(\d+)')
                        assert int(halted[1]) > 0
                        assert b'acpi-global-lock: acquire=0 release=0 held=2 after=0 denied=1 errors=0' in complete_lines(output)
                if args.fault not in ('none','handoff'):
                    assert b'acpi-global-lock: PASS' not in complete_lines(output)
                    assert b'acpi-runtime: stopped=yes' not in complete_lines(output)
                    regs = qmp.call('human-monitor-command', {'command-line': 'info registers'})
                    flags = re.search(r'RFL=([0-9a-fA-F]+)', regs)
                    assert flags and not (int(flags[1], 16) & 0x200), regs
                else:
                    if args.fault=='handoff':
                        assert complete_lines(output).count(b'acpi-global-lock: modeled-handoff handled=1 cleared=1')==2
                        assert complete_lines(output).count(b'acpi-global-lock: firmware-wait')==2
                    assert complete_lines(output).count(b'acpi-global-lock: acquire=0 release=0 held=2 after=0 denied=0 errors=0') == 2
                qmp.call('quit')
                process.wait(timeout=10)
            finally:
                stop_process(process)
                (out/'serial.txt').write_bytes(output)
    print(f'acpi-global-lock: PASS mode={args.fault}; Q35 fixture only, no native firmware acceptance')


if __name__ == '__main__':
    main()
