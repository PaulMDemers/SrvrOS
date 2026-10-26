#!/usr/bin/env python3
"""Exercise bounded AHCI waits with USB input on a disposable, throttled disk."""
import argparse
import json
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time

import uefi_usb_smoke as usb
from hardware_boot_smoke import wait_for as boot_wait_for


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=usb.default_ovmf_path())
    parser.add_argument('--boot-wait', type=float, default=180)
    parser.add_argument('--dump-memory', action='store_true',
                        help='Save guest RAM along with the scratch disk on failure')
    parser.add_argument('--relaunch-only', type=int, default=0, metavar='CYCLES',
                        help='Only repeat fsck, shell launch and executable comparison')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    logs = root / 'build/review'
    logs.mkdir(parents=True, exist_ok=True)
    serial = logs / 'storage-service-serial.log'
    serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='srvros-storage-service-') as temp_name:
        temp = Path(temp_name)
        disk = temp / 'scratch.exfat'
        shutil.copyfile(root / 'build/srvros.exfat', disk)
        qmp_path = temp / 'qmp.sock'
        command = [args.qemu, '-M', 'q35', '-m', '512M',
                   '-drive', f'if=pflash,format=raw,readonly=on,file={args.ovmf}',
                   '-cdrom', str(root / 'build/srvros-x86_64.iso'), '-boot', 'd',
                   '-drive', f'if=none,id=scratch,format=raw,file={disk}',
                   '-device', 'ich9-ahci,id=testahci', '-device', 'ide-hd,drive=scratch,bus=testahci.0',
                   '-device', 'qemu-xhci,id=xhci', '-device', 'usb-kbd,bus=xhci.0',
                   '-serial', f'file:{serial}', '-qmp', f'unix:{qmp_path},server=on,wait=off',
                   '-display', 'none', '-monitor', 'none', '-no-reboot']
        with (logs / 'storage-service-qemu.log').open('wb') as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            capture_failure = None
            failed = False

            def wait_for(*values):
                try:
                    return boot_wait_for(*values)
                except Exception:
                    if capture_failure is not None:
                        try:
                            capture_failure()
                        except Exception as error:
                            print(f'Could not capture guest RAM: {error}', flush=True)
                    raise

            try:
                text = wait_for(serial, lambda s: s.endswith('srv> '), process, args.boot_wait)
                if 'exfat: mounted /fat from ahci' not in text:
                    raise RuntimeError('Test did not mount the disposable AHCI disk')
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as qmp:
                    qmp.settimeout(10)
                    qmp.connect(str(qmp_path))
                    stream = qmp.makefile('rb')
                    json.loads(stream.readline())  # Greeting.
                    sequence = 0

                    def call(_sock, request):
                        nonlocal sequence
                        sequence += 1
                        request = dict(request, id=sequence)
                        qmp.sendall(json.dumps(request).encode() + b'\n')
                        while True:
                            line = stream.readline()
                            if not line:
                                raise RuntimeError('QMP disconnected')
                            result = json.loads(line)
                            if result.get('id') != sequence:
                                continue
                            if 'error' in result:
                                raise RuntimeError(f'QMP {request["execute"]}: {result["error"]}')
                            return result.get('return')

                    usb.qmp_command = call
                    call(qmp, {'execute': 'qmp_capabilities'})

                    def capture_failure():
                        if args.dump_memory:
                            call(qmp, {'execute': 'stop'})
                            call(qmp, {'execute': 'pmemsave', 'arguments': {
                                'val': 0, 'size': 512 * 1024 * 1024,
                                'filename': str(logs / 'storage-service-failed.ram')}})

                    def send(text):
                        usb.send_qmp_text(qmp, text, 0.045)

                    def current():
                        return serial.read_text(errors='replace')

                    def shell(command, marker=None, seconds=60):
                        offset = len(current())
                        send(command + '\n')
                        return wait_for(serial, lambda s: s.endswith(' $ ') and
                                        (marker is None or marker in s[offset:]), process, seconds)

                    def throttle(iops=0, bps=0):
                        call(qmp, {'execute': 'block_set_io_throttle', 'arguments': {
                            'device': 'scratch', 'bps': 0, 'bps_rd': 0, 'bps_wr': bps,
                            'iops': 0, 'iops_rd': 0, 'iops_wr': iops}})

                    def relaunch_checks(cycles):
                        for iteration in range(cycles):
                            offset = len(current())
                            send('fsck /fat\n')
                            wait_for(serial, lambda s: 'errors=0 ok' in s[offset:] and
                                     s.endswith('srv> '), process, 60)
                            send('run /fat/bin/sh\n')
                            wait_for(serial, lambda s: s.endswith(' $ '), process, 30)
                            shell('cmp /sh /fat/bin/sh && echo executable-ok', '\nexecutable-ok\n')
                            send('exit\n')
                            wait_for(serial, lambda s: s.endswith('srv> '), process, 20)
                            print(f'storage-relaunch: {iteration + 1}/{cycles}', flush=True)

                    if args.relaunch_only > 0:
                        relaunch_checks(args.relaunch_only)
                        if usb.has_fatal_exception(current()):
                            raise RuntimeError('Kernel exception during relaunch test')
                        print('storage-relaunch: PASS (fsck, shell launch, executable comparison)')
                        return

                    send('latency reset\n')
                    wait_for(serial, lambda s: 'latency: reset active=yes' in s and s.endswith('srv> '), process, 20)
                    send('run /fat/bin/sh\n')
                    wait_for(serial, lambda s: s.endswith(' $ '), process, 20)
                    throttle(iops=40)
                    offset = len(current())
                    send('dd if=/dev/zero of=/fat/io-stress.bin bs=4096 count=16\n')
                    wait_for(serial, lambda s: '/fat/bin/dd entry=' in s[offset:], process, 30)
                    time.sleep(0.15)
                    if current().endswith(' $ '):
                        raise RuntimeError('Write finished before input could overlap the disk wait')
                    # A whole command must be retained while the shell is paused.
                    send('echo disk-keys-ok\n')
                    wait_for(serial, lambda s: '\ndisk-keys-ok\n' in s[offset:] and s.endswith(' $ '), process, 60)
                    throttle()
                    shell('stat /fat/io-stress.bin', '65536 bytes')
                    shell('cp /fat/io-stress.bin /fat/io-stress-copy.bin')
                    shell('cmp /fat/io-stress.bin /fat/io-stress-copy.bin && echo io-copy-ok', '\nio-copy-ok\n')

                    # Cancellation must not abandon request ownership or DMA memory.
                    throttle(iops=40)
                    offset = len(current())
                    send('dd if=/dev/zero of=/fat/io-cancel.bin bs=4096 count=16\n')
                    wait_for(serial, lambda s: '/fat/bin/dd entry=' in s[offset:], process, 30)
                    time.sleep(0.15)
                    if current().endswith(' $ '):
                        raise RuntimeError('Cancellation missed the active write')
                    for down in (True, False):
                        call(qmp, {'execute': 'input-send-event', 'arguments': {'events': [
                            {'type': 'key', 'data': {'down': down, 'key': {'type': 'qcode', 'data': key}}}
                            for key in ('ctrl', 'c')]}})
                        time.sleep(0.3)
                    wait_for(serial, lambda s: 'status 130' in s[offset:] and s.endswith(' $ '), process, 60)
                    throttle()
                    shell('echo after-cancel', '\nafter-cancel\n')
                    shell('sync')
                    send('exit\n')
                    wait_for(serial, lambda s: s.endswith('srv> '), process, 20)
                    send('fsck /fat\n')
                    wait_for(serial, lambda s: 'errors=0 ok' in s and s.endswith('srv> '), process, 60)
                    send('block\n')
                    text = wait_for(serial, lambda s: 'ahci: commands=' in s and s.endswith('srv> '), process, 30)
                    match = re.search(r'ahci: commands=(\d+) service_windows=(\d+) busy_rejects=0 failures=0', text)
                    if not match or int(match[2]) == 0:
                        raise RuntimeError('Disk operations did not service other threads cleanly')
                    send('latency off\n')
                    text = wait_for(serial, lambda s: 'latency: usb-poll-gap' in s and s.endswith('srv> '), process, 20)
                    for name in ('ahci', 'timer-gap', 'usb-poll-gap'):
                        sample = re.search(r'latency: ' + name + r' count=(\d+)', text)
                        if sample is None or int(sample[1]) == 0:
                            raise RuntimeError(f'Missing {name} progress samples')
                    time.sleep(0.2)
                    call(qmp, {'execute': 'screendump', 'arguments': {
                        'filename': str(logs / 'storage-service.ppm')}})

                    # A background supervisor also reads the disk. Repeated
                    # cache churn followed by executable loads caught a race
                    # after the driver's IRQ restore but before cache publish.
                    relaunch_checks(3)

                    # On this disposable disk only, force a command beyond its
                    # deadline. The disk must quarantine and return to the shell.
                    send('run /fat/bin/sh\n')
                    wait_for(serial, lambda s: s.endswith(' $ '), process, 20)
                    throttle(bps=64)
                    offset = len(current())
                    send('write /fat/io-timeout.txt timeout\n')
                    time.sleep(0.15)
                    send('echo timeout-keys-ok\n')
                    text = wait_for(serial, lambda s: 'failed; disk offline' in s[offset:] and
                                    '\ntimeout-keys-ok\n' in s[offset:] and s.endswith(' $ '), process, 60)
                    timeout = re.search(
                        r'failed; disk offline phase=completion reason=deadline '
                        r'lba=\d+ elapsed_us=(\d+) spins=\d+ windows=\d+ '
                        r'ci=(0x[0-9a-f]+) is=(0x[0-9a-f]+) tfd=0x[0-9a-f]+ serr=0x[0-9a-f]+',
                        text[offset:])
                    if (timeout is None or int(timeout[1]) < 2000000 or
                            not (int(timeout[2], 16) & 1) or (int(timeout[3], 16) & (1 << 30))):
                        raise RuntimeError('Forced pending command lacks the expected deadline/register receipt')
                    throttle()
                    offset = len(current())
                    text = shell('sync')
                    failure = text[offset:]
                    if 'status 0' in failure or not ('status 1' in failure or 'sh: exec failed' in failure):
                        raise RuntimeError('Offline storage unexpectedly allowed sync to succeed')
                    send('exit\n')
                    wait_for(serial, lambda s: s.endswith('srv> '), process, 20)
                    offset = len(current())
                    send('block\n')
                    text = wait_for(serial, lambda s: 'ahci: commands=' in s[offset:] and s.endswith('srv> '), process, 20)
                    match = re.search(r'ahci: commands=\d+ service_windows=\d+ busy_rejects=0 failures=(\d+)', text[offset:])
                    if match is None or int(match[1]) != 1:
                        raise RuntimeError('Failed disk was not quarantined after exactly one failed command')
                    if usb.has_fatal_exception(text):
                        raise RuntimeError('Kernel exception during storage test')
            except Exception:
                # Keep the disposable image for postmortem checks; never lose
                # the evidence merely because TemporaryDirectory cleans up.
                failed = True
                raise
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                if failed:
                    shutil.copyfile(disk, logs / 'storage-service-failed.exfat')
    print('storage-service-smoke: PASS (throttled writes, USB typing, Ctrl-C, copy comparison, fsck, timeout/quarantine)')
    print(f'Serial log: {serial}')


if __name__ == '__main__':
    main()
