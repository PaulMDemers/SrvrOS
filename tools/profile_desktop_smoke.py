#!/usr/bin/env python3
"""Exercise portable profile migration across two disposable EFI installations.

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
import profile_bundle
import profile_image
import source_provenance

ROOT = Path(__file__).resolve().parents[1]
SOURCE = b'portable source document'
EDITED = b'portable profile edited document'
BINARY = b'\x00\xffportable\x80'
ID = '12345678-1234-4234-8234-123456789abc'


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


def boot(disk, out, number, args, home, speed, offset_minutes, edit=False):
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
                        qmp.key({' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus', '$': '4', '>': 'dot', '_': 'minus'}.get(char, char.lower()), char.isupper() or char in '$>_')

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
                if b'exfat: mounted /fat from ahci0p2 ' not in output or b'trying initramfs fallback' in output:
                    raise RuntimeError('Installed GPT partition is not the sole /fat backing')
                # Refusal controls must return to the console before the valid session.
                for invalid in ('/fat/home/not-provisioned', '/fat/home/bad-marker', '/fat/home/../bad'):
                    serial.sendall(('run /fat/bin/deskd --profile=' + invalid + '\n').encode())
                    wait(b'deskd: profile unavailable or invalid;'); wait(b'srv> ')
                launch_start = len(output)
                serial.sendall(('run /fat/bin/deskd --profile=' + home + ' --apps-trace --scale=1\n').encode())
                wait(b'deskd: display physical=1280x800 logical=1280x800 scale=1')
                wait(b'desktools: starting role=text'); wait(b'deskd: commit')
                for marker in (f'deskd: profile id={ID} home={home} credentials=unchanged'.encode(),
                               f'offset={offset_minutes} flags=3 wallpaper=0 pointer_speed={speed}'.encode()):
                    if marker not in output[launch_start:]: wait(marker)
                expected = SOURCE if edit else EDITED
                shortcut('ctrl-l'); shortcut('ctrl-a'); text(home + '/Documents/migration.txt')
                shortcut('ctrl-o'); wait(b'picker ready mode=1')
                text('migration'); qmp.key('ret'); wait(b'file-result role=text op=1 status=0')
                shortcut('ctrl-a'); shortcut('ctrl-c')
                wait(f'clipboard role=2 action=8 bytes={len(expected)}'.encode())
                if edit:
                    text(EDITED.decode()); shortcut('ctrl-s'); wait(b'file-result role=text op=2 status=0')
                    click(790, 750); wait(b'desktools: starting role=settings')
                    wait(b'deskd: geometry pid='); wait(b'\n')
                    geometry = re.findall(rb'deskd: geometry pid=(\d+) window=\d+ frame=(\d+),(\d+),(\d+),(\d+)', output)[-1]
                    _, sx, sy, sw, _ = map(int, geometry)
                    column = (sw - 2 - 64) // 3
                    click(sx + 1 + 24 + column + 15, sy + 44 + 172 + 16)
                    wait(b'theme_accent=0 glass_level=1 theme_style=1'); wait(b'status=1 ')
                else:
                    if b'theme_accent=0 glass_level=1 theme_style=1' not in output:
                        wait(b'theme_accent=0 glass_level=1 theme_style=1')
                qmp.call('screendump', {'filename': str(out / f'text-{number}.ppm')})
                click(390, 750); wait(b'desktools: starting role=files')
                wait(b'file-result role=files op=3 status=0')
                if ('files-location path=' + home).encode() not in output:
                    wait(('files-location path=' + home).encode())
                qmp.call('screendump', {'filename': str(out / f'files-{number}.ppm')})
                shortcut('meta_l-spc'); text('Terminal'); qmp.key('ret'); wait(b'deskterm: ready')
                time.sleep(1)
                text('env > $HOME/Documents/environment.txt'); qmp.key('ret'); time.sleep(2)
                text('pwd > $HOME/Documents/cwd.txt'); qmp.key('ret'); time.sleep(2)
                qmp.call('screendump', {'filename': str(out / f'terminal-{number}.ppm')})
                text('exit'); qmp.key('ret'); time.sleep(2)
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


def machine_preferences(speed, offset):
    return (f'srvros-desktop-v7\ntext_size=16\nutc_offset_minutes={offset}\nclock_flags=3\n'
            f'wallpaper=0\npointer_speed={speed}\npointer_acceleration=1\n'
            'disable_while_typing=0\ntap_to_click=1\ntwo_finger_tap=1\n'
            'theme_accent=0\nglass_level=1\ntheme_style=0\ndisplay_scale=1\n'
            'display_width=1280\ndisplay_height=800\n').encode()


def make_bundle():
    import base64
    def doc(path, data):
        return {'path': path, 'size': len(data), 'sha256': digest(data),
                'data_base64': base64.b64encode(data).decode()}
    return {'format': profile_bundle.FORMAT, 'schema_version': 1,
            'profile': {'id': ID, 'display_name': 'Portable profile'},
            'preferences': {'text_size': 16, 'clock_flags': 3, 'wallpaper': 0,
                            'theme_accent': 0, 'glass_level': 1, 'theme_style': 0},
            'documents': [doc('Documents/migration.txt', SOURCE), doc('Documents/binary.dat', BINARY)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True)
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    if not 10 <= args.boot_wait <= 240: parser.error('invalid boot wait')
    for firmware in (args.ovmf, args.ovmf_vars):
        if not Path(firmware).is_file() or ',' in firmware: parser.error('invalid firmware path')
    base = ROOT / 'build/review/profile-desktop'; base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    print(f'Profile desktop evidence: {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'source': source_provenance.capture(ROOT),
               'firmware_sha256': digest(Path(args.ovmf).read_bytes()),
               'firmware_vars_sha256': digest(Path(args.ovmf_vars).read_bytes()),
               'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0],
               'tool_sha256': {name: digest((ROOT / 'tools' / name).read_bytes()) for name in
                               ('profile_desktop_smoke.py', 'profile_bundle.py', 'profile_image.py',
                                'installed_disk_smoke.py', 'installation_image.py')}}
    save_json(out / 'receipt.json', receipt)
    try:
        receipt['build'] = verified_build(ROOT)
        driver = ROOT / 'build/tests/exfat-driver'
        profile_driver = ROOT / 'build/tests/profile-image-driver'
        base_volume = ROOT / 'build/srvros.exfat'
        receipt['boots'] = {}
        receipt['stores'] = {}
        receipt['installations'] = {}
        bundle = make_bundle()
        stores = []
        for installation, home, alias, uid, gid, speed, offset_minutes in (
                ('a', '/fat/home/local-a', 'local_a', 1001, 1001, 4, -300),
                ('b', '/fat/home/local-b', 'local_b', 2050, 2051, 2, 60)):
            store = out / ('store-' + installation); profile_bundle.init_store(store)
            local = profile_bundle.import_bundle(store, bundle, alias, uid, gid)
            stores.append(local)
            receipt['stores'][installation] = {'root': str(store), **local}
            volume = out / ('provisioned-' + installation + '.exfat')
            seed_machine = machine_preferences(speed, offset_minutes)
            profile_image.provision(base_volume, volume, bundle, home, alias, uid, gid, profile_driver,
                                    seed_machine)
            # The write adapter accepts only its private, single-link staging image.
            # Complete the malformed-marker home so startup reaches the marker check.
            with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=out) as temporary:
                temporary = Path(temporary); image = temporary / 'image'
                shutil.copyfile(volume, image)
                for directory in ('', '/Documents', '/.config', '/.config/srvros',
                                  '/.local', '/.local/share', '/.cache'):
                    subprocess.run([str(profile_driver), str(image), 'mkdir',
                                    '/fat/home/bad-marker' + directory], check=True)
                marker = temporary / 'marker'; marker.write_bytes(b'bad marker\n')
                prefs = temporary / 'prefs'; prefs.write_bytes(b'{}\n')
                for path, content in (('/fat/home/bad-marker/profile.id', marker),
                                      ('/fat/home/bad-marker/.config/srvros/portable.json', prefs)):
                    subprocess.run([str(profile_driver), str(image), 'write', path, str(content)], check=True)
                stage = out / ('target-' + installation)
                installation_image.prepare(stage, ROOT / 'build/kernel/srvros.elf',
                    ROOT / 'build/limine-binary/BOOTX64.EFI', image, ROOT / 'build/userspace/init.elf')
                ready = installation_image.apply(stage)
            disk = stage / 'image.img'
            _, (offset, size) = partitions(disk)
            bounds = outside_data(disk, offset, size)
            if installation == 'b' and ready['gpt']['disk_guid'] == receipt['installations']['a']['gpt']['disk_guid']:
                raise RuntimeError('installation disk GUIDs are not distinct')
            receipt['installations'][installation] = {'disk': str(disk), 'gpt': ready['gpt'], 'data_offset': offset,
                'data_bytes': size, 'protected_regions': bounds, 'initial_disk_sha256': digest(disk.read_bytes()),
                'provisioned_volume_sha256': digest(volume.read_bytes())}
            for iteration in (1, 2):
                number = installation + str(iteration)
                boot(disk, out, number, args, home, speed, offset_minutes, edit=(installation == 'a' and iteration == 1))
                if outside_data(disk, offset, size) != bounds: raise RuntimeError('outside partition changed')
                payload = data_image(disk, offset, size)
                extracted = out / ('data-' + number + '.exfat'); extracted.write_bytes(payload)
                result = subprocess.run([str(driver), str(extracted), 'allocation-check'], capture_output=True, text=True)
                (out / ('allocation-' + number + '.log')).write_text(result.stdout + result.stderr)
                if result.returncode or 'leaked=0 fat_bitmap_mismatches=0 errors=0' not in result.stdout:
                    raise RuntimeError('offline allocation check failed')
                fixture = ExfatFixture(payload); prefix = home.removeprefix('/fat/') + '/'
                document = fixture.read(prefix + 'Documents/migration.txt')
                binary = fixture.read(prefix + 'Documents/binary.dat')
                if document != EDITED: raise RuntimeError('document mismatch')
                if binary != BINARY: raise RuntimeError('binary document mismatch')
                portable = fixture.read(prefix + profile_bundle.PREF_PATH)
                if profile_bundle.decode_json(portable).get('theme_style') != 1: raise RuntimeError('theme mismatch')
                machine = fixture.read('home/desktop.conf')
                if machine != seed_machine: raise RuntimeError('machine preferences changed')
                env = fixture.read(prefix + 'Documents/environment.txt').decode()
                cwd = fixture.read(prefix + 'Documents/cwd.txt').decode().strip()
                expected = {'HOME': home, 'SRVROS_DESKTOP_HOME': home, 'SRVROS_PROFILE_ID': ID,
                            'XDG_CONFIG_HOME': home + '/.config', 'XDG_DATA_HOME': home + '/.local/share',
                            'XDG_CACHE_HOME': home + '/.cache'}
                if cwd != home or any(k + '=' + v not in env.splitlines() for k, v in expected.items()):
                    raise RuntimeError('terminal environment or cwd mismatch')
                receipt['boots'][number] = {'profile_id': ID, 'home': home, 'alias': alias, 'uid': uid, 'gid': gid,
                    'document_sha256': digest(document), 'binary_sha256': digest(binary),
                    'portable_preferences_sha256': digest(portable), 'machine_preferences_sha256': digest(machine),
                    'environment_sha256': digest(env.encode()), 'cwd': cwd, 'data_sha256': digest(payload)}
                save_json(out / 'receipt.json', receipt)
                if installation == 'a' and iteration == 2:
                    host_home = Path(local['home'])
                    (host_home / 'Documents/migration.txt').write_bytes(document)
                    (host_home / 'Documents/binary.dat').write_bytes(binary)
                    (host_home / profile_bundle.PREF_PATH).write_bytes(portable)
                    bundle = profile_bundle.export_bundle(store, ID, ['Documents/migration.txt', 'Documents/binary.dat'])
                    profile_bundle.publish_bundle(out / 'export.json', bundle)
            receipt['disk_' + installation + '_sha256'] = digest(disk.read_bytes())
        receipt.update(status='PASS',
                       limits=['disposable QEMU images only', 'no authentication or access-control claim',
                               'no physical disk or power-loss acceptance', 'external fsck.exfat not run'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        save_json(out / 'receipt.json', receipt)
    print(f'profile-desktop: PASS; evidence {out}', flush=True)


if __name__ == '__main__': main()
