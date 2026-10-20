#!/usr/bin/env python3
"""Run the battery diagnostic runtime with a disposable Q35 EC model."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from vm_sockets import connect_unix
from serial_records import complete_lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=('normal', 'denied', 'transport', 'storm', 'deadline', 'query-deadline', 'disconnect-deadline', 'foreign-event', 'foreign-restore-fail', 'foreign-mask-fail', 'foreign-reenable', 'native-refusal'), default='normal')
    args = parser.parse_args()
    faults = {'normal': 0, 'denied': 1, 'transport': 2, 'storm': 3, 'deadline': 4, 'query-deadline':5, 'disconnect-deadline':6, 'native-refusal': 0, 'foreign-event': 7, 'foreign-restore-fail': 8, 'foreign-mask-fail': 9, 'foreign-reenable': 10}
    root = Path(__file__).resolve().parents[1]
    out = root/'build/review'/('acpi-battery-' + args.case)
    out.mkdir(parents=True, exist_ok=True)
    output = bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-battery-') as directory:
        tmp = Path(directory)
        source = (root/'tools/fixtures/acpi/battery-runtime.asl').read_text()
        (tmp/'battery.asl').write_text('#define BATTERY_FAULT ' + str(faults[args.case]) + '\n' + source)
        subprocess.run(['iasl', '-p', str(tmp/'battery'), str(tmp/'battery.asl')], check=True, stdout=subprocess.DEVNULL)
        kernel = root/'build/acpi-battery'/('srvros.elf' if args.case == 'native-refusal' else 'vm.elf')
        iso = boot.build_iso(str(root), str(tmp), 1280, 800, os.environ.copy(), kernel)
        command = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', iso,
                   '-boot', 'd', '-nic', 'none', '-acpitable', f'file={tmp}/battery.aml',
                   '-serial', f'unix:{tmp}/serial,server=on,wait=off', '-qmp',
                   f'unix:{tmp}/qmp,server=on,wait=off', '-display', 'none',
                   '-monitor', 'none', '-no-reboot']
        with (out/'qemu.txt').open('w') as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                serial = connect_unix(tmp/'serial', 20); serial.settimeout(.05)
                qmp = QMP(tmp/'qmp')
                start = time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start < 60:
                    output.extend(boot.read_for(serial, .2))
                    qmp.boot_if_menu_stalled(output, time.monotonic()-start)
                assert b'srv> ' in output, bytes(output)
                for _ in range(2 if args.case in ('normal','foreign-event') else 1):
                    part = bytearray(); serial.sendall(b'acpiruntime battery\n')
                    start = time.monotonic()
                    while time.monotonic()-start < 100:
                        part.extend(boot.read_for(serial, .1))
                        lines = complete_lines(part)
                        if b'battery: PASS' in lines or b'battery: HALTED' in lines or b'battery: REFUSED' in lines:
                            break
                    output.extend(part); (out/'serial.txt').write_bytes(output)
                    if args.case in ('normal','foreign-event'):
                        assert b'battery: PASS samples=5 notifications=2 queries=1 attached=1 detached=1 heap=0 errors=0 denied=0' in part, bytes(part)
                        samples = re.findall(rb'battery: sample=(\d+) ac_present=1 soc=86 voltage_uv=12000000 current_ua=-250000 capacity=4300000 full=5000000', part)
                        assert list(map(int,samples)) == [1,2,3,4,5], bytes(part)
                        if args.case == 'foreign-event':
                            assert b'battery-vm: foreign restored enable=0x000000000000062f pending=0x0000000000000040' in part, bytes(part)
                            assert b'battery-vm: foreign restored enable=0x000000000000062e pending=0x0000000000000004' in part, bytes(part)
                            assert part.count(b'battery: foreign-event held ')==2,bytes(part)
                            assert len(re.findall(rb'battery: foreign-event restored .*protected_writes=[1-9][0-9]*',part))==2,bytes(part)
                            assert re.search(rb'foreign-event restored .*protected_writes=[1-9][0-9]*',part),bytes(part)
                            assert b'ERROR foreign' not in part,bytes(part)
                    elif args.case == 'native-refusal':
                        assert b'battery: REFUSED platform/resource ownership preflight' in part, bytes(part)
                        assert b'battery: phase=' not in part
                        assert b'battery-native: REFUSED check=pci-gpu' in part
                    else:
                        reasons = {'denied': b'namespace/EC connection', 'transport': b'namespace/EC connection', 'storm': b'query limit', 'deadline': b'operation deadline', 'query-deadline':b'independent query deadline', 'disconnect-deadline':b'operation deadline', 'foreign-restore-fail':b'foreign event restore', 'foreign-mask-fail':b'foreign event mask', 'foreign-reenable':b'foreign event unexpectedly enabled'}
                        assert b'battery: HALTED '+reasons[args.case] in part, bytes(part)
                        assert b'battery: PASS' not in part
                        assert b'ERROR foreign' not in part
                        if args.case == 'foreign-restore-fail':
                            assert part.count(b'battery: foreign-event restored ')==1,bytes(part)
                            assert b'battery-vm: foreign restored enable=0x000000000000062f pending=0x0000000000000040' in part,bytes(part)
                        if args.case == 'foreign-mask-fail':
                            assert part.count(b'battery: foreign-event held ')==1 and b'battery: phase=' not in part,bytes(part)
                        if args.case == 'deadline':
                            assert 4.5 <= time.monotonic()-start < 9
                        output.extend(boot.read_for(serial, 3 if args.case in ('deadline','query-deadline','disconnect-deadline') else .3))
                        registers = qmp.call('human-monitor-command', {'command-line': 'info registers'})
                        flags = re.search(r'RFL=([0-9a-fA-F]+)', registers)
                        assert flags and not(int(flags[1],16)&0x200), registers
                qmp.call('quit'); process.wait(timeout=10)
            finally:
                stop_process(process); (out/'serial.txt').write_bytes(output)
    print('acpi-battery: PASS '+args.case+' (VM evidence only)')


if __name__ == '__main__':
    main()
