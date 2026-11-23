#!/usr/bin/env python3
"""Exercise the one-page GGTT mapping core in an isolated SrvrOS Q35 guest."""
import argparse
import re
import shutil
import socket
import subprocess
import tarfile
import tempfile
import time
from pathlib import Path

from browser_runtime_smoke import receive, receive_run
from test_scanout_transfer import OWNED_PAGE_MODES

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
FIXTURE = BUILD / 'tests/scanout-guest'


def make_iso(directory, program="ownedpage"):
    tree = directory / 'tree'
    boot = tree / 'boot'
    config = boot / 'limine'
    config.mkdir(parents=True)
    limine = BUILD / 'limine-binary'
    for name in ('limine-bios.sys', 'limine-bios-cd.bin'):
        shutil.copyfile(limine / name, config / name)
    shutil.copyfile(BUILD / 'kernel/srvros.elf', boot / 'srvros.elf')
    (config / 'limine.conf').write_text(
        'timeout: 0\nserial: yes\n/owned-page\n    protocol: limine\n'
        '    path: boot():/boot/srvros.elf\n    cmdline: srvros.log=quiet\n'
        '    module_path: boot():/boot/initramfs.tar\n    module_string: initramfs\n')
    with tarfile.open(boot / 'initramfs.tar', 'w', format=tarfile.USTAR_FORMAT) as archive:
        archive.add(BUILD / 'userspace/init.elf', arcname='init')
        archive.add(FIXTURE / program, arcname=program)
    iso = directory / 'owned-page.iso'
    subprocess.run(['xorriso', '-as', 'mkisofs', '-R', '-r', '-J', '-b',
                    'boot/limine/limine-bios-cd.bin', '-no-emul-boot', '-boot-load-size',
                    '4', '-boot-info-table', '-o', iso, tree], check=True)
    subprocess.run([limine / 'limine', 'bios-install', iso], check=True)
    return iso


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--blitter', action='store_true', help='Run the Broadwell command driver fixture')
    args = parser.parse_args()
    program = 'blitter' if args.blitter else 'ownedpage'
    cases = [('', 'PASS Broadwell blitter driver (30 cases)'),
             ('warm-lost-completion', 'PASS warm mapping retains lost completion'),
             ('cold-init-fault', 'PASS cold initialization retains selected status page'),
             ('cold-rollback-fault', 'PASS cold rollback retains changed reset epoch')] if args.blitter else [
        (mode, f'PASS {mode}') for mode in OWNED_PAGE_MODES]
    if args.blitter:
        cases.extend((mode, f'PASS {mode}') for mode in
                     ('stale-seventh', 'swapped-translations', 'unposted-invalidate', 'stale-restore',
                      'missing-engine-ack', 'early-l3', 'missing-l3', 'widi-escapes',
                      'changed-traffic-epoch', 'moving-scanout', 'missing-rollback-ack',
                      'air-reset-reasons', 'partial-gam-go', 'unowned-widi-clock', 'wireless-audio-reopens', 'uncertain-release'))
    with tempfile.TemporaryDirectory(prefix='srvros-owned-page-') as temp:
        directory = Path(temp)
        iso = make_iso(directory, program)
        serial = directory / 'serial'
        cmd = [args.qemu, '-M', 'q35', '-m', '512', '-boot', 'd', '-cdrom', iso,
               '-display', 'none', '-monitor', 'none', '-nic', 'none', '-no-reboot',
               '-serial', f'unix:{serial},server=on,wait=on']
        with (FIXTURE / ('blitter-serial.log' if args.blitter else 'serial.log')).open('wb') as log, socket.socket(socket.AF_UNIX) as sock:
            proc = subprocess.Popen(cmd)
            try:
                end = time.monotonic() + 10
                while not serial.exists() and proc.poll() is None and time.monotonic() < end:
                    time.sleep(.05)
                sock.connect(str(serial))
                sock.settimeout(.25)
                boot = receive(sock, b'srv> ', 45)
                log.write(boot)
                if b'srv> ' not in boot:
                    raise RuntimeError('SrvrOS monitor did not start')
                for mode, expected in cases:
                    sock.sendall(f'run /{program} {mode}\n'.encode())
                    result = receive_run(sock, 25)
                    log.write(result)
                    if (expected.encode() not in result.splitlines() or
                            not re.search(rb'^run: pid=\d+ exited status=0\r?$', result, re.M)):
                        raise RuntimeError(f'{mode} failed; see {log.name}')
                    print(expected, flush=True)
            finally:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()


if __name__ == '__main__':
    main()
