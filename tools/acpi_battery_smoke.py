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
    parser.add_argument('--case', choices=('normal', 'denied', 'transport', 'storm', 'deadline', 'query-deadline', 'disconnect-deadline', 'foreign-event', 'foreign-restore-fail', 'foreign-mask-fail', 'foreign-reenable', 'mode-button', 'mode-unexpected', 'transitions', 'transition-timeout', 'native-refusal', 'service', 'service-stale', 'service-incoherent', 'service-refusal', 'service-query-deadline'), default='normal')
    args = parser.parse_args()
    faults = {'normal': 0, 'denied': 1, 'transport': 2, 'storm': 3, 'deadline': 4, 'query-deadline':5, 'disconnect-deadline':6, 'native-refusal': 0, 'foreign-event': 7, 'foreign-restore-fail': 8, 'foreign-mask-fail': 9, 'foreign-reenable': 10, 'mode-button':11, 'mode-unexpected':12, 'transitions':13, 'transition-timeout':14, 'service':15, 'service-stale':16, 'service-incoherent':17, 'service-refusal':0, 'service-query-deadline':5}
    transitions=args.case in ('transitions','transition-timeout')
    service=args.case.startswith('service')
    root = Path(__file__).resolve().parents[1]
    out = root/'build/review'/('acpi-battery-' + args.case)
    out.mkdir(parents=True, exist_ok=True)
    output = bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-battery-') as directory:
        tmp = Path(directory)
        source = (root/'tools/fixtures/acpi/battery-runtime.asl').read_text()
        (tmp/'battery.asl').write_text('#define BATTERY_FAULT ' + str(faults[args.case]) + '\n' + source)
        subprocess.run(['iasl', '-p', str(tmp/'battery'), str(tmp/'battery.asl')], check=True, stdout=subprocess.DEVNULL)
        kernel = root/'build/acpi-battery'/('srvros.elf' if args.case in ('native-refusal','service-refusal') else 'vm.elf')
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
                for iteration in range(2 if args.case in ('normal','foreign-event','mode-button','transition-timeout') else 1):
                    # After a human timeout the ownership lifetime must be reusable.
                    reentry=args.case=='transition-timeout' and iteration==1
                    part = bytearray()
                    command=(b'battery-service-refusal' if args.case=='service-refusal' else
                             b'battery-service-stale' if args.case in ('service-stale','service-incoherent') else
                             b'battery-service' if service else b'battery-transitions' if transitions and not reentry else b'battery')
                    serial.sendall(b'acpiruntime '+command+b'\n')
                    start = time.monotonic()
                    while time.monotonic()-start < (155 if transitions else 100):
                        part.extend(boot.read_for(serial, .1))
                        lines = complete_lines(part)
                        if service:
                            if b'battery-service-test: PASS' in lines or b'battery-service-test: FAIL' in lines or b'battery: HALTED' in lines:break
                            continue
                        if b'battery-transition: PASS' in lines or b'battery-transition: INCOMPLETE' in lines or b'battery: PASS' in lines or b'battery: HALTED' in lines or b'battery: REFUSED' in lines:
                            break
                    output.extend(part); (out/'serial.txt').write_bytes(output)
                    if service and args.case!='service-query-deadline':
                        assert b'battery-service-test: PASS' in part and b'battery-service-test: FAIL' not in part,bytes(part)
                        assert b'battery: HALTED' not in part and b'ERROR foreign' not in part,bytes(part)
                        if args.case=='service-refusal':
                            assert b'battery-service-test: PASS unavailable' in part and b'battery: phase=' not in part,bytes(part)
                        else:
                            assert b'busy_reads=0 ' not in part and b'; retired=yes' in part,bytes(part)
                            assert part.count(b'battery-service: retired ')>=2,bytes(part)
                            assert part.count(b'battery: foreign-event restored ')>=4,bytes(part)
                            if args.case in ('service-stale','service-incoherent'):
                                assert b'stale=1 recovered=1' in part,bytes(part)
                            else:assert b'rejected=0 ' in part,bytes(part)
                    elif reentry:
                        assert re.search(rb'battery: PASS samples=5 notifications=\d+ queries=\d+ attached=1 detached=1 heap=0 errors=0 denied=0',part),bytes(part)
                        assert part.count(b'battery: foreign-event restored ')==2,bytes(part)
                        assert b'battery: HALTED' not in part and b'ERROR foreign' not in part,bytes(part)
                    elif transitions:
                        expected=b'PASS phases=3' if args.case=='transitions' else b'INCOMPLETE phases=1'
                        assert b'battery-transition: '+expected in part,bytes(part)
                        assert b'heap=0 errors=0 denied=0; retired=yes' in part,bytes(part)
                        assert part.count(b'battery: foreign-event restored ')==2,bytes(part)
                        assert b'battery: HALTED' not in part and b'ERROR foreign' not in part,bytes(part)
                        if args.case=='transitions':
                            assert b'samples=9 ' in part,bytes(part)
                            assert part.count(b'battery-transition: accepted=')==3,bytes(part)
                            assert b'UNPLUG AC now' in part and b'RECONNECT AC now' in part,bytes(part)
                        else:
                            assert time.monotonic()-start>=120,bytes(part)
                            assert b'RECONNECT AC now' not in part,bytes(part)
                    elif args.case in ('normal','foreign-event','mode-button'):
                        assert b'battery: PASS samples=5 notifications=2 queries=1 attached=1 detached=1 heap=0 errors=0 denied=0' in part, bytes(part)
                        samples = re.findall(rb'battery: sample=(\d+) ac_present=1 soc=86 voltage_uv=12000000 current_ua=-250000 capacity=4300000 full=5000000', part)
                        assert list(map(int,samples)) == [1,2,3,4,5], bytes(part)
                        if args.case == 'mode-button':
                            assert b'battery: post-mode fixed-enable=0x0000000000000100' in part,bytes(part)
                            assert b'battery: post-mode power button masked; status not acknowledged' in part,bytes(part)
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
                        reasons = {'denied': b'namespace/EC connection', 'transport': b'namespace/EC connection', 'storm': b'query limit', 'deadline': b'operation deadline', 'query-deadline':b'independent query deadline', 'disconnect-deadline':b'operation deadline', 'foreign-restore-fail':b'foreign event restore', 'foreign-mask-fail':b'foreign event mask', 'foreign-reenable':b'foreign event unexpectedly enabled', 'mode-unexpected':b'unexpected post-mode fixed enable', 'service-query-deadline':b'independent query deadline'}
                        assert b'battery: HALTED '+reasons[args.case] in part, bytes(part)
                        assert b'battery: PASS' not in part
                        assert b'ERROR foreign' not in part
                        if args.case == 'mode-unexpected':
                            assert b'battery: post-mode fixed-enable=0x0000000000000200' in part and b'battery: phase=' not in part,bytes(part)
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
