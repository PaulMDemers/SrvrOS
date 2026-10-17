#!/usr/bin/env python3
"""USB BOT + MBR + exFAT integration on disposable QEMU images only."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import time

import uefi_usb_smoke as usb
from hardware_boot_smoke import wait_for


def make_iso(root, temp, writable, extra_cmdline=""):
    tree = temp / ('rw-root' if writable else 'ro-root')
    shutil.copytree(root / 'build/iso_root', tree)
    (tree / 'boot/limine/limine.conf').write_text(
        'timeout: 0\nserial: yes\n/srvros\n    protocol: limine\n'
        '    path: boot():/boot/srvros.elf\n'
        '    cmdline: srvros.log=debug srvros.storage=ram' +
        (' srvros.usb-storage=rw' if writable else '') + extra_cmdline + '\n'
        '    module_path: boot():/boot/initramfs.tar\n    module_string: initramfs\n')
    iso = temp / ('rw.iso' if writable else 'ro.iso')
    subprocess.run(['xorriso', '-as', 'mkisofs', '-R', '-r', '-J',
                    '-b', 'boot/limine/limine-bios-cd.bin', '-no-emul-boot',
                    '-boot-load-size', '4', '-boot-info-table',
                    '--efi-boot', 'boot/limine/limine-uefi-cd.bin',
                    '-efi-boot-part', '--efi-boot-image', '--protective-msdos-label',
                    str(tree), '-o', str(iso)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return iso


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=usb.default_ovmf_path())
    parser.add_argument('--timeout-only', action='store_true', help='Only exercise a stalled read and device quarantine')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    logs = root / 'build/review'
    logs.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='srvros-usb-storage-') as directory:
        temp = Path(directory)
        disk = temp / 'scratch-mbr.img'
        source = root / 'build/srvros.exfat'
        mbr = bytearray(512)
        mbr[446+4] = 7
        struct.pack_into('<II', mbr, 446+8, 2048, source.stat().st_size // 512)
        mbr[510:] = b'\x55\xaa'
        with disk.open('wb') as out, source.open('rb') as src:
            out.write(mbr)
            out.seek(2048*512)
            shutil.copyfileobj(src, out)
        ro_iso, rw_iso = make_iso(root, temp, False), make_iso(root, temp, True)
        digest = lambda: hashlib.sha256(disk.read_bytes()).hexdigest()

        def run(name, writable, persisted=False, timeout_test=False):
            serial = logs / f'usb-storage-{name}.log'
            serial.unlink(missing_ok=True)
            qmp_path = temp / 'qmp.sock'
            qmp_path.unlink(missing_ok=True)
            cmd = [args.qemu, '-M', 'q35', '-m', '512M',
                   '-drive', f'if=pflash,format=raw,readonly=on,file={args.ovmf}',
                   '-cdrom', str(rw_iso if writable else ro_iso), '-boot', 'd',
                   '-device', 'qemu-xhci,id=xhci', '-device', 'usb-kbd,bus=xhci.0',
                   '-drive', f'if=none,id=scratch,format=raw,file={disk}',
                   '-device', 'usb-storage,id=stick,drive=scratch,bus=xhci.0',
                   '-serial', f'file:{serial}', '-qmp', f'unix:{qmp_path},server=on,wait=off',
                   '-display', 'none', '-monitor', 'none', '-no-reboot']
            with (logs / f'usb-storage-{name}-qemu.log').open('wb') as err:
                process = subprocess.Popen(cmd, stdout=err, stderr=err)
                try:
                    text = wait_for(serial, lambda s: s.endswith('srv> '), process, 180)
                    for marker in ('usb-storage: usb0 blocks=', 'block: registered MBR partition usb0p1',
                                   'storage: RAM-only mode; AHCI disabled', 'exfat: mounted /fat from initramfs-exfat'):
                        if marker not in text:
                            raise RuntimeError(f'Missing {marker}; see {serial}')
                    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as qmp:
                        qmp.settimeout(10)
                        qmp.connect(str(qmp_path))
                        stream = qmp.makefile('rb')
                        json.loads(stream.readline())
                        sequence = 0

                        def call(_sock, request):
                            nonlocal sequence
                            sequence += 1
                            qmp.sendall(json.dumps(dict(request, id=sequence)).encode() + b'\n')
                            while True:
                                response = json.loads(stream.readline())
                                if response.get('id') == sequence:
                                    if 'error' in response: raise RuntimeError(response)
                                    return response.get('return')
                        usb.qmp_command = call
                        call(qmp, {'execute': 'qmp_capabilities'})

                        def command(text, marker, prompt='srv> ', timeout=90):
                            offset = len(serial.read_text(errors="replace"))
                            usb.send_qmp_text(qmp, text+'\n', 0.025)
                            result = wait_for(serial, lambda s: usb.has_fatal_exception(s) or
                                              (marker in s[offset:] and s.endswith(prompt)), process, timeout)
                            if usb.has_fatal_exception(result): raise RuntimeError(f'Kernel exception; see {serial}')
                            return result

                        command('echo aAzZ-storage-keyboard', '\naAzZ-storage-keyboard\n')
                        command('mount usb0p1 /usb', 'mounted')
                        command('fsck /usb', 'errors=0 ok')
                        command('run /sh', ' $ ', prompt=' $ ')
                        if timeout_test:
                            call(qmp, {'execute': 'block_set_io_throttle', 'arguments': {
                                'device': 'scratch', 'bps': 0, 'bps_rd': 64, 'bps_wr': 0,
                                'iops': 0, 'iops_rd': 0, 'iops_wr': 0}})
                            offset = len(serial.read_text(errors='replace'))
                            usb.send_qmp_text(qmp, 'cmp /sh /usb/bin/sh\n', 0.025)
                            time.sleep(0.15)
                            usb.send_qmp_text(qmp, 'echo usb-timeout-keys-ok\n', 0.025)
                            wait_for(serial, lambda s: 'device quarantined' in s[offset:] and
                                     '\nusb-timeout-keys-ok\n' in s[offset:] and s.endswith(' $ '), process, 30)
                            command('exit', 'srv> ')
                            command('xhci', 'offline=1')
                            command('echo usb-offline-keyboard-ok', '\nusb-offline-keyboard-ok\n')
                            print('usb-storage: timeout PASS (bounded quarantine, keyboard progress)', flush=True)
                            return
                        command('cmp /sh /usb/bin/sh && echo usb-read-ok', '\nusb-read-ok\n', prompt=' $ ')
                        if persisted:
                            command('cat /usb/usb-log.txt', 'USB-persistent-log-123', prompt=' $ ')
                        if writable:
                            # Retain a whole USB keyboard command while the disk
                            # is deliberately too slow to finish before typing.
                            call(qmp, {'execute': 'block_set_io_throttle', 'arguments': {
                                'device': 'scratch', 'bps': 0, 'bps_rd': 0, 'bps_wr': 0,
                                'iops': 0, 'iops_rd': 0, 'iops_wr': 40}})
                            offset = len(serial.read_text(errors="replace"))
                            usb.send_qmp_text(qmp, 'dd if=/dev/zero of=/usb/io-stress.bin bs=4096 count=16\n', 0.025)
                            wait_for(serial, lambda s: '/fat/bin/dd entry=' in s[offset:], process, 30)
                            time.sleep(0.15)
                            if serial.read_text().endswith(' $ '):
                                raise RuntimeError('Missed overlap with disk write')
                            usb.send_qmp_text(qmp, 'echo usb-disk-keys-ok\n', 0.025)
                            wait_for(serial, lambda s: '\nusb-disk-keys-ok\n' in s[offset:] and s.endswith(' $ '), process, 90)
                            call(qmp, {'execute': 'block_set_io_throttle', 'arguments': {
                                'device': 'scratch', 'bps': 0, 'bps_rd': 0, 'bps_wr': 0,
                                'iops': 0, 'iops_rd': 0, 'iops_wr': 0}})
                            command('stat /usb/io-stress.bin', '65536 bytes', prompt=' $ ')
                            command('echo USB-persistent-log-123 > /usb/usb-log.txt', ' $ ', prompt=' $ ')
                            command('sync && echo usb-sync-ok', '\nusb-sync-ok\n', prompt=' $ ')
                            command('cat /usb/usb-log.txt', 'USB-persistent-log-123', prompt=' $ ')
                        command('exit', 'srv> ')
                        if not writable:
                            command('write /usb/should-not-exist.txt denied', 'write: failed')
                        command('fsck /usb', 'errors=0 ok')
                        command('echo usb-final-keyboard-ok', '\nusb-final-keyboard-ok\n')
                        if usb.has_fatal_exception(serial.read_text(errors='replace')):
                            raise RuntimeError('Kernel exception')
                    print(f'usb-storage: {name} PASS', flush=True)
                finally:
                    process.terminate()
                    try: process.wait(10)
                    except subprocess.TimeoutExpired:
                        process.kill(); process.wait()

        before = digest()
        if args.timeout_only:
            run('timeout', False, timeout_test=True)
            if digest() != before: raise RuntimeError('Timeout test changed read-only disk')
            return
        run('read-only', False)
        if digest() != before: raise RuntimeError('Read-only boot changed scratch disk')
        run('write', True)
        if digest() == before: raise RuntimeError('Write boot did not change scratch disk')
        before = digest()
        run('reload', False, persisted=True)
        if digest() != before: raise RuntimeError('Reload boot changed scratch disk')
        print('usb-storage: PASS (real USB transport, MBR, read-only enforcement, exFAT write/flush, persistence, keyboard)')


if __name__ == '__main__':
    main()
