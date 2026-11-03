#!/usr/bin/env python3
"""Prove a disposable single-disk EFI installation and fresh-boot persistence.

Build first. All evidence and images are retained. Never accepts a user disk.
Only QEMU processes created by this invocation are stopped.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tarfile
import tempfile
import time

from desktop_foundation_smoke import QMP
from desktop_theme_profiles_smoke import qmp_axis
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from uefi_usb_smoke import default_ovmf_path
import installation_image
import source_provenance

ROOT = Path(__file__).resolve().parents[1]
PHRASE = b'installed disk persistence proof'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def save_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def verified_build(root):
    """Require recorded artifacts and the exact current source/submodule state."""
    try:
        build = json.loads((root / 'build/macos-build.json').read_text())
        current = source_provenance.capture(root)
        if (build['schema'] != 1 or build['source'] != current['.']['head'] or
            build['worktree_dirty'] != current['.']['dirty'] or
            build['tracked_diff_sha256'] != current['.']['tracked_diff_sha256'] or
            build['repositories'] != current):
            raise ValueError('source or submodule state differs from build record')
        for name in ('build/kernel/srvros.elf', 'build/initramfs.tar', 'build/srvros.exfat'):
            if digest((root / name).read_bytes()) != build['hashes'][name]:
                raise ValueError(f'artifact differs from build record: {name}')
        return build
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise RuntimeError(f'Run sh tools/macos/build.sh -j2 all for a matched build first: {error}') from error


def partitions(disk):
    """Read only the two generated entries, never discover host disks."""
    import struct
    with disk.open('rb') as stream:
        stream.seek(1024)
        table = stream.read(256)
    result = []
    for offset in (0, 128):
        first, last = struct.unpack_from('<QQ', table, offset + 32)
        if not 34 <= first <= last < disk.stat().st_size // 512 - 33:
            raise RuntimeError('Partition outside selected image bounds')
        result.append((first * 512, (last - first + 1) * 512))
    if result[0][0] + result[0][1] > result[1][0]:
        raise RuntimeError('Overlapping partitions')
    return result


def outside_data(disk, offset, size):
    with disk.open('rb') as stream:
        prefix = digest(stream.read(offset))
        stream.seek(offset + size)
        suffix = digest(stream.read())
    return {'before_data_sha256': prefix, 'after_data_sha256': suffix}


def data_image(disk, offset, size):
    with disk.open('rb') as stream:
        stream.seek(offset)
        return stream.read(size)


def boot(disk, out, number, args, negative=False):
    output = bytearray()
    consumed = 0
    serial = qmp = None
    with tempfile.TemporaryDirectory(prefix='srv-install-', dir='/tmp') as tmp:
        qmp_path = Path(tmp) / 'qmp'
        port = choose_port(30001, 38000)
        variables = out / f'firmware-vars-{number}.fd'
        shutil.copyfile(args.ovmf_vars, variables)
        command = [args.qemu, '-machine', 'q35,i8042=off', '-accel', 'tcg',
                   '-m', '1024M', '-smp', '1', '-nic', 'none',
                   '-drive', f'if=pflash,format=raw,readonly=on,file={args.ovmf}',
                   '-drive', f'if=pflash,format=raw,file={variables}',
                   '-drive', f'if=none,id=installed,format=raw,file={disk}',
                   '-device', 'ich9-ahci,id=ahci',
                   '-device', 'ide-hd,drive=installed,bus=ahci.0,bootindex=1',
                   '-vga', 'none', '-device', 'VGA,vgamem_mb=64,xres=1280,yres=800',
                   '-device', 'virtio-keyboard-pci,disable-legacy=on',
                   '-device', 'virtio-tablet-pci,disable-legacy=on',
                   '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off',
                   '-qmp', f'unix:{qmp_path},server=on,wait=off',
                   '-display', 'none', '-monitor', 'none', '-no-reboot']
        # Firmware is the only host-supplied boot component. The sole disk must
        # supply the OS loader, kernel, bootstrap, apps and persisted state.
        assert not {'-cdrom', '-kernel', '-initrd', '-bios'} & set(command)
        assert command.count('-drive') == 3  # code flash, variable flash, one disk
        save_json(out / f'command-{number}.json', command)
        with (out / f'qemu-{number}.log').open('wb') as log:
            vm = subprocess.Popen(command, stdout=log, stderr=log)
            try:
                serial = connect_serial(port, 20)
                serial.settimeout(.2)
                qmp = QMP(qmp_path)

                def wait(marker, timeout=60):
                    nonlocal consumed
                    start = time.monotonic()
                    while marker not in output[consumed:]:
                        if time.monotonic() - start > timeout or vm.poll() is not None:
                            raise RuntimeError(f'boot {number}: missing {marker!r}')
                        try:
                            chunk = serial.recv(16384)
                            if not chunk:
                                raise RuntimeError('Serial disconnected')
                            output.extend(chunk)
                            (out / f'serial-{number}.log').write_bytes(output)
                        except socket.timeout:
                            pass
                    consumed = output.index(marker, consumed) + len(marker)

                def shortcut(keys):
                    qmp.call('human-monitor-command', {'command-line': 'sendkey ' + keys})
                    time.sleep(.3)

                def text(value):
                    for char in value:
                        qmp.key({' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus'}.get(char, char.lower()), char.isupper())

                def click(x, y):
                    qmp.call('input-send-event', {'events': [
                        {'type': 'abs', 'data': {'axis': 'x', 'value': qmp_axis(x, 1280)}},
                        {'type': 'abs', 'data': {'axis': 'y', 'value': qmp_axis(y, 800)}}]})
                    time.sleep(.2)
                    for down in (True, False):
                        qmp.call('input-send-event', {'events': [
                            {'type': 'btn', 'data': {'button': 'left', 'down': down}}]})
                        time.sleep(.2)

                wait(b'srv> ', args.boot_wait)
                if b'init: readiness=ready' not in output:
                    raise RuntimeError('Installed bootstrap init did not become ready')
                if negative:
                    if b'exfat: mounted /fat' in output:
                        raise RuntimeError('Damaged data partition unexpectedly mounted')
                    if b'exfat: initramfs fallback /srvros.exfat not found' not in output:
                        raise RuntimeError('Negative control did not reach missing-fallback refusal')
                    serial.sendall(b'elf /fat/bin/deskd\n')
                    wait(b'elf: not found: /fat/bin/deskd')
                    wait(b'\r\nsrv> ')
                    return
                if b'exfat: mounted /fat from ahci0p2 ' not in output or b'trying initramfs fallback' in output:
                    raise RuntimeError('Installed GPT partition is not the sole /fat backing')
                if number == 2:
                    serial.sendall(b'cat /fat/home/installed-proof.txt\n')
                    wait(PHRASE); wait(b'srv> ')
                    serial.sendall(b'cat /fat/home/desktop.conf\n')
                    wait(b'theme_style=1'); wait(b'srv> ')
                serial.sendall(b'run /fat/bin/deskd --apps-trace --scale=1\n')
                wait(b'deskd: display physical=1280x800 logical=1280x800 scale=1')
                wait(b'desktools: starting role=text'); wait(b'deskd: commit')
                if number == 1:
                    text(PHRASE.decode())
                    shortcut('ctrl-l'); shortcut('ctrl-a'); text('/fat/home/installed-proof.txt')
                    shortcut('ctrl-s'); wait(b'picker ready mode=2'); qmp.key('ret')
                    wait(b'file-result role=text op=2 status=0')
                    click(790, 750)
                    wait(b'desktools: starting role=settings')
                    wait(b'deskd: geometry pid='); wait(b'\n')
                    geometry = re.findall(rb'deskd: geometry pid=(\d+) window=\d+ frame=(\d+),(\d+),(\d+),(\d+)', output)[-1]
                    _, sx, sy, sw, _ = map(int, geometry)
                    column = (sw - 2 - 64) // 3
                    click(sx + 1 + 24 + column + 15, sy + 44 + 172 + 16)
                    wait(b'theme_accent=0 glass_level=1 theme_style=1'); wait(b'status=1 ')
                else:
                    if b'theme_accent=0 glass_level=1 theme_style=1' not in output:
                        wait(b'theme_accent=0 glass_level=1 theme_style=1')
                    shortcut('ctrl-l'); shortcut('ctrl-a'); text('/fat/home/installed-proof.txt')
                    shortcut('ctrl-o'); wait(b'picker ready mode=1')
                    text('installed-proof'); qmp.key('ret')
                    wait(b'file-result role=text op=1 status=0')
                    shortcut('ctrl-a'); shortcut('ctrl-c')
                    wait(f'clipboard role=2 action=8 bytes={len(PHRASE)}'.encode())
                time.sleep(.5)
                qmp.call('screendump', {'filename': str(out / f'desktop-{number}.ppm')})
                shortcut('f10'); shortcut('end'); shortcut('ret')
                wait(b'deskd: exited'); wait(b'srv> ')
                serial.sendall(b'fsck /fat\n'); wait(b'exfat-check:'); wait(b'srv> ')
                if not re.search(rb'exfat-check: mounts=1 files=\d+ clusters=\d+ leaks=0 fat=0 errors=0 ok', output):
                    raise RuntimeError('Guest allocation check failed')
                if b'exception:' in output or b'failure=1' in output or b'deskd: FAIL' in output:
                    raise RuntimeError('Guest reported failure')
                # Normal guest reboot flushes all storage; -no-reboot turns that
                # request into process exit, followed by a genuinely fresh VM.
                serial.sendall(b'reboot\n')
                vm.wait(timeout=30)
                if vm.returncode != 0:
                    raise RuntimeError(f'Guest reboot exit status {vm.returncode}')
            except Exception:
                if qmp and vm.poll() is None:
                    try:
                        qmp.call('screendump', {'filename': str(out / f'failure-{number}.ppm')})
                    except Exception:
                        pass
                raise
            finally:
                if serial:
                    serial.close()
                if qmp:
                    qmp.stream.close(); qmp.sock.close()
                stop_process(vm)
                (out / f'serial-{number}.log').write_bytes(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True, help='matching blank OVMF variable-store template; copied for every boot')
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    if not 10 <= args.boot_wait <= 240:
        parser.error('--boot-wait must be between 10 and 240')
    if not Path(args.ovmf).is_file() or ',' in args.ovmf:
        parser.error('supply a regular firmware file via --ovmf (no commas)')
    if not Path(args.ovmf_vars).is_file() or ',' in args.ovmf_vars:
        parser.error('supply a regular variable-store template via --ovmf-vars (no commas)')
    base = ROOT / 'build/review/installed-disk'
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    print(f'Installed-disk evidence: {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'source': subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'source_status': subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
        'submodules': subprocess.check_output(['git', 'submodule', 'status', '--recursive'], cwd=ROOT, text=True),
        'firmware_sha256': digest(Path(args.ovmf).read_bytes()),
        'firmware_vars_template_sha256': digest(Path(args.ovmf_vars).read_bytes()),
        'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]}
    receipt['tool_sha256'] = {name: digest((ROOT / 'tools' / name).read_bytes()) for name in
                             ('installation_image.py', 'installed_disk_smoke.py', 'mk_usb_image.py')}
    save_json(out / 'receipt.json', receipt)
    try:
        receipt['build'] = verified_build(ROOT)
        with tarfile.open(ROOT / 'build/initramfs.tar') as archive:
            member = next(m for m in archive.getmembers() if m.name.lstrip('./') == 'init')
            if archive.extractfile(member).read() != runtime_elf((ROOT / 'build/userspace/init.elf').read_bytes()):
                raise RuntimeError('Bootstrap init differs from matched build archive')
        stage = out / 'target'
        installation_image.prepare(stage, ROOT / 'build/kernel/srvros.elf',
            ROOT / 'build/limine-binary/BOOTX64.EFI', ROOT / 'build/srvros.exfat',
            ROOT / 'build/userspace/init.elf')
        installation_image.apply(stage)
        with tarfile.open(stage / 'bootstrap.tar') as archive:
            if archive.getnames() != ['init']:
                raise RuntimeError('Installation bootstrap contains more than /init')
        receipt['bootstrap_members'] = ['init']
        disk = stage / 'image.img'
        receipt['installed_sha256'] = digest(disk.read_bytes())
        _, (offset, size) = partitions(disk)
        bounds = outside_data(disk, offset, size)
        fixture = ExfatFixture(data_image(disk, offset, size))
        for binary in ('deskd', 'desktools'):
            expected = runtime_elf((ROOT / f'build/userspace/{binary}.elf').read_bytes())
            if fixture.read('bin/' + binary) != expected:
                raise RuntimeError(f'Installed {binary} differs from built application')
        saved = None
        for number in (1, 2):
            boot(disk, out, number, args)
            payload = data_image(disk, offset, size)
            fixture = ExfatFixture(payload)
            if fixture.read('home/installed-proof.txt') != PHRASE:
                raise RuntimeError('Saved Text document bytes differ')
            preferences = fixture.read('home/desktop.conf')
            if b'theme_style=1\n' not in preferences:
                raise RuntimeError('Theme preference was not persisted')
            if saved is not None and preferences != saved:
                raise RuntimeError('Preference bytes changed after fresh boot')
            saved = preferences
            if outside_data(disk, offset, size) != bounds:
                raise RuntimeError('Guest wrote outside installed data partition')
            extracted = out / f'data-{number}.exfat'
            extracted.write_bytes(payload)
            driver = ROOT / 'build/tests/exfat-driver'
            result = subprocess.run([str(driver), str(extracted), 'allocation-check'], capture_output=True, text=True)
            (out / f'allocation-{number}.log').write_text(result.stdout + result.stderr)
            if result.returncode or 'leaked=0 fat_bitmap_mismatches=0 errors=0' not in result.stdout:
                raise RuntimeError('Offline allocation check failed')
        # Retain a separate negative fixture; destroy only its exFAT boot copies.
        bad = out / 'missing-system.img'
        shutil.copyfile(disk, bad)
        with bad.open('r+b') as stream:
            for sector in (0, 12):
                stream.seek(offset + sector * 512); stream.write(bytes(512))
        boot(bad, out, 'negative', args, negative=True)
        receipt.update(status='PASS', document_sha256=digest(PHRASE),
                       preferences_sha256=digest(saved), data_bounds={'offset': offset, 'bytes': size},
                       protected_regions=bounds, final_disk_sha256=digest(disk.read_bytes()),
                       checks=['single disk EFI boot', 'minimal init bootstrap without RAM disk/apps',
                               'disk-loaded Text save and Settings preference', 'guest reboot with flush',
                               'fresh VM exact file readback and Text reopen', 'preference restoration',
                               'two guest/offline allocation checks', 'unchanged bytes outside data',
                               'damaged system volume refuses app with no RAM fallback'],
                       limits=['VM image provisioning only', 'no physical disk or power-cut acceptance',
                               'external fsck.exfat not run', 'no multi-disk root selection or in-place upgrade'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        save_json(out / 'receipt.json', receipt)
    print(f'installed-disk: PASS; evidence {out}', flush=True)


if __name__ == '__main__':
    main()
