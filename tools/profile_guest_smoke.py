#!/usr/bin/env python3
"""Prove guest profile export, recovery, import and activation on disposable EFI disks.

Run after a source-matched build. Images and evidence are retained; no host disk is
accepted and no profile payload is injected into the transport by the host.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import socket
import subprocess
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
import profile_desktop_smoke as d
import profile_image
import source_provenance

ROOT = Path(__file__).resolve().parents[1]
A = '/fat/home/local-a'
B = '/fat/home/local-b'
TRANSPORT = '/work/transfer.json'


def check_allocation(disk, out, label):
    _, (offset, size) = d.partitions(disk)
    payload = d.data_image(disk, offset, size)
    image = out / f'data-{label}.exfat'
    image.write_bytes(payload)
    result = subprocess.run([str(ROOT / 'build/tests/exfat-driver'), str(image), 'allocation-check'],
                            capture_output=True, text=True)
    (out / f'allocation-{label}.log').write_text(result.stdout + result.stderr)
    if result.returncode or 'leaked=0 fat_bitmap_mismatches=0 errors=0' not in result.stdout:
        raise RuntimeError(f'{label}: offline allocation check failed')
    return ExfatFixture(payload)


def boot(disk, transport, out, label, args, stage, checkpoint=None):
    output = bytearray()
    consumed = 0
    serial = qmp = None
    with tempfile.TemporaryDirectory(prefix='srv-profile-guest-', dir='/tmp') as tmp:
        qmp_path = Path(tmp) / 'qmp'
        port = choose_port(30001, 38000)
        variables = out / f'firmware-vars-{label}.fd'
        shutil.copyfile(args.ovmf_vars, variables)
        command = [args.qemu, '-machine', 'q35,i8042=off', '-accel', 'tcg', '-m', '1024M',
                   '-smp', '1', '-nic', 'none',
                   '-drive', f'if=pflash,format=raw,readonly=on,file={args.ovmf}',
                   '-drive', f'if=pflash,format=raw,file={variables}',
                   '-device', 'ich9-ahci,id=ahci',
                   '-drive', f'if=none,id=installed,format=raw,file={disk}',
                   '-device', 'ide-hd,drive=installed,bus=ahci.0,bootindex=1',
                   '-drive', f'if=none,id=transport,format=raw,file={transport}',
                   '-device', 'ide-hd,drive=transport,bus=ahci.1,bootindex=2',
                   '-vga', 'none', '-device', 'VGA,vgamem_mb=64,xres=1280,yres=800',
                   '-device', 'virtio-keyboard-pci,disable-legacy=on',
                   '-device', 'virtio-tablet-pci,disable-legacy=on',
                   '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off',
                   '-qmp', f'unix:{qmp_path},server=on,wait=off',
                   '-display', 'none', '-monitor', 'none', '-no-reboot']
        assert not {'-cdrom', '-kernel', '-initrd', '-bios'} & set(command)
        d.save_json(out / f'command-{label}.json', command)
        with (out / f'qemu-{label}.log').open('wb') as log:
            vm = subprocess.Popen(command, stdout=log, stderr=log)
            try:
                serial = connect_serial(port, 20); serial.settimeout(.2)
                qmp = QMP(qmp_path)
                def wait(marker, timeout=60):
                    nonlocal consumed
                    start = time.monotonic()
                    while marker not in output[consumed:]:
                        if time.monotonic() - start > timeout or vm.poll() is not None:
                            raise RuntimeError(f'{label}: missing {marker!r}')
                        try:
                            chunk = serial.recv(16384)
                            if not chunk: raise RuntimeError('serial disconnected')
                            output.extend(chunk)
                            (out / f'serial-{label}.log').write_bytes(output)
                        except socket.timeout:
                            pass
                    consumed = output.index(marker, consumed) + len(marker)
                def command_run(line, success=True, stop_after=None, program=None):
                    start = len(output)
                    prefix = (program + ' ') if program else (('/fat/bin/env SRVROS_PROFILE_STOP_AFTER=' + stop_after + ' profile ') if stop_after else '/fat/bin/profile ')
                    monitor_line = 'run ' + prefix + line
                    # The monitor reserves one byte of its 128-byte line buffer for NUL.
                    if len(monitor_line.encode('ascii')) >= 128:
                        raise RuntimeError(f'{label}: monitor command exceeds line limit')
                    serial.sendall((monitor_line + '\n').encode())
                    wait(b' exited status='); wait(b'\nsrv> ')
                    match = re.search(rb'run: pid=\d+ exited status=(\d+)', output[start:])
                    if (not match or (int(match.group(1)) == 0) != success or
                        (stop_after and int(match.group(1)) != 86)):
                        raise RuntimeError(f'{label}: unexpected status for profile {line}')
                def shortcut(keys):
                    qmp.call('human-monitor-command', {'command-line': 'sendkey ' + keys}); time.sleep(.3)
                def text(value):
                    for char in value:
                        qmp.key({' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus'}.get(char, char.lower()), char.isupper())
                def click(x, y):
                    qmp.call('input-send-event', {'events': [
                        {'type': 'abs', 'data': {'axis': 'x', 'value': qmp_axis(x, 1280)}},
                        {'type': 'abs', 'data': {'axis': 'y', 'value': qmp_axis(y, 800)}}]})
                    time.sleep(.2)
                    for down in (True, False):
                        qmp.call('input-send-event', {'events': [{'type': 'btn', 'data': {'button': 'left', 'down': down}}]})
                        time.sleep(.2)
                def desktop(home, edit):
                    wait(b'deskd: display physical=1280x800 logical=1280x800 scale=1')
                    wait(b'desktools: starting role=text'); wait(b'deskd: commit')
                    marker = f'deskd: profile id={d.ID} home={home} credentials=unchanged'.encode()
                    if marker not in output: wait(marker)
                    shortcut('ctrl-l'); shortcut('ctrl-a'); text(home + '/Documents/migration.txt')
                    shortcut('ctrl-o'); wait(b'picker ready mode=1'); text('migration'); qmp.key('ret')
                    wait(b'file-result role=text op=1 status=0')
                    shortcut('ctrl-a'); shortcut('ctrl-c')
                    wait(f'clipboard role=2 action=8 bytes={len(d.SOURCE if edit else d.EDITED)}'.encode())
                    if edit:
                        text(d.EDITED.decode()); shortcut('ctrl-s'); wait(b'file-result role=text op=2 status=0')
                        click(790, 750); wait(b'desktools: starting role=settings')
                        wait(b'deskd: geometry pid='); wait(b'\n')
                        geometry = re.findall(rb'deskd: geometry pid=(\d+) window=\d+ frame=(\d+),(\d+),(\d+),(\d+)', output)[-1]
                        _, sx, sy, sw, _ = map(int, geometry)
                        click(sx + 1 + 24 + (sw - 2 - 64) // 3 + 15, sy + 44 + 172 + 16)
                        wait(b'theme_accent=0 glass_level=1 theme_style=1'); wait(b'status=1 ')
                    elif b'theme_accent=0 glass_level=1 theme_style=1' not in output:
                        wait(b'theme_accent=0 glass_level=1 theme_style=1')
                    qmp.call('screendump', {'filename': str(out / f'desktop-{label}.ppm')})
                    shortcut('f10'); shortcut('end'); shortcut('ret')
                    wait(b'deskd: exited'); wait(b'srv> ')
                wait(b'srv> ', args.boot_wait)
                if (b'init: readiness=ready' not in output or
                    b'exfat: mounted /fat from ahci0p2 ' not in output or
                    b'installed-root: selected device=ahci0p2 format=exfat-v1 role=system' not in output or
                    b'trying initramfs fallback' in output or
                    b'block: registered GPT partition ahci1p2 ' not in output):
                    raise RuntimeError(f'{label}: installed root or transport GPT selection failed')
                serial.sendall(b'mount ahci1p2 /work\n'); wait(b'mount: ahci1p2 on /work type=exfat'); wait(b'srv> ')
                if stage == 'export':
                    serial.sendall(('run /fat/bin/deskd --profile=' + A + ' --apps-trace --scale=1\n').encode())
                    desktop(A, True)
                    command_run(f'export {A} {TRANSPORT} Documents/migration.txt Documents/binary.dat')
                    command_run(f'inspect {TRANSPORT}')
                elif stage == 'interrupt':
                    probe = B
                    command_run(f'import {TRANSPORT} {probe} local_b 2050 2051', success=False, stop_after=checkpoint)
                elif stage == 'recover':
                    probe = B
                    command_run(f'activate {probe}', success=False)
                    if checkpoint == 'journal':
                        # The test operator creates ambiguous data after the
                        # journal but before transaction ownership is established.
                        command_run(probe, program='/fat/bin/mkdir')
                        command_run(probe + '/.profile-copy-temp preserve-me', program='/fat/bin/write')
                        command_run(f'recover {probe}', success=False)
                        if b'scratch without pending marker' not in output:
                            raise RuntimeError('ambiguous scratch was not refused')
                        serial.sendall(('cat ' + probe + '/.profile-copy-temp\n').encode())
                        wait(b'preserve-me'); wait(b'srv> ')
                        # Explicit operator cleanup of this synthetic fixture;
                        # recover itself must leave the unknown bytes untouched.
                        command_run(probe + '/.profile-copy-temp', program='/fat/bin/rm')
                    command_run(f'recover {probe}')
                elif stage == 'import':
                    command_run(f'inspect {TRANSPORT}')
                    command_run(f'import {TRANSPORT} {B} local_b 2050 2051')
                elif stage == 'activate':
                    activation_start = len(output)
                    serial.sendall((f'run /fat/bin/profile activate {B}\n').encode())
                    desktop(B, False)
                    if not re.search(rb'run: pid=\d+ exited status=0', output[activation_start:]):
                        raise RuntimeError(f'{label}: activation process failed')
                for mount in ('/work', '/fat'):
                    start = len(output); serial.sendall(('fsck ' + mount + '\n').encode()); wait(b'exfat-check:'); wait(b'srv> ')
                    if not re.search(rb'exfat-check: mounts=1 files=\d+ clusters=\d+ leaks=0 fat=0 errors=0 ok', output[start:]):
                        raise RuntimeError(f'{label}: guest allocation check failed for {mount}')
                serial.sendall(b'unmount /work\n'); wait(b'exfat: unmounted /work '); wait(b'srv> ')
                if any(s in output for s in (b'exception:', b'failure=1', b'deskd: FAIL')):
                    raise RuntimeError(f'{label}: guest reported failure')
                serial.sendall(b'reboot\n'); vm.wait(timeout=30)
                if vm.returncode: raise RuntimeError(f'{label}: reboot exit status {vm.returncode}')
            except Exception:
                if qmp and vm.poll() is None:
                    try: qmp.call('screendump', {'filename': str(out / f'failure-{label}.ppm')})
                    except Exception: pass
                raise
            finally:
                if serial: serial.close()
                if qmp: qmp.stream.close(); qmp.sock.close()
                stop_process(vm)
                (out / f'serial-{label}.log').write_bytes(output)


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
    base = ROOT / 'build/review/profile-guest'; base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    print(f'Profile guest evidence: {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'source': source_provenance.capture(ROOT),
               'firmware_sha256': d.digest(Path(args.ovmf).read_bytes()),
               'firmware_vars_sha256': d.digest(Path(args.ovmf_vars).read_bytes()),
               'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0],
               'tool_sha256': {name: d.digest((ROOT / 'tools' / name).read_bytes()) for name in
                   ('profile_guest_smoke.py', 'profile_desktop_smoke.py', 'profile_bundle.py',
                    'profile_image.py', 'installation_image.py')}}
    d.save_json(out / 'receipt.json', receipt)
    try:
        receipt['build'] = d.verified_build(ROOT)
        profile_driver = ROOT / 'build/tests/profile-image-driver'
        base_volume = ROOT / 'build/srvros.exfat'
        machine_a = d.machine_preferences(4, -300)
        machine_b = d.machine_preferences(2, 60)
        source = out / 'source.exfat'
        profile_image.provision(base_volume, source, d.make_bundle(), A, 'local_a', 1001, 1001,
                                profile_driver, machine_a)
        # B starts with no imported profile; only its machine-scoped settings are seeded.
        target = out / 'destination.exfat'
        with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=out) as tmp:
            image = Path(tmp) / 'image'; shutil.copyfile(base_volume, image)
            prefs = Path(tmp) / 'machine'; prefs.write_bytes(machine_b)
            for operation, path, extra in (('ensure-home', '/fat/home', []),
                                           ('write-desktop', '/fat/home/desktop.conf', [str(prefs)]),
                                           ('check-clean', '/fat/home', [])):
                subprocess.run([str(profile_driver), str(image), operation, path, *extra], check=True)
            shutil.copyfile(image, target)
        receipt['installations'] = {}
        disks = {}
        bounds = {}
        for name, volume in (('a', source), ('b', target), ('transport', base_volume)):
            stage = out / ('target-' + name)
            installation_image.prepare(stage, ROOT / 'build/kernel/srvros.elf',
                ROOT / 'build/limine-binary/BOOTX64.EFI', volume,
                ROOT / 'build/userspace/init.elf', identity_bound=True)
            ready = installation_image.apply(stage)
            disk = stage / 'image.img'; disks[name] = disk
            _, (offset, size) = d.partitions(disk)
            bounds[name] = d.outside_data(disk, offset, size)
            receipt['installations'][name] = {'disk': str(disk), 'gpt': ready['gpt'],
                'data_offset': offset, 'data_bytes': size, 'protected_regions': bounds[name],
                'initial_disk_sha256': d.digest(disk.read_bytes())}
        guids = [item['gpt']['disk_guid'] for item in receipt['installations'].values()]
        if len(set(guids)) != 3: raise RuntimeError('disk GUIDs are not distinct')
        initial_b = check_allocation(disks['b'], out, 'b-initial')
        try: initial_b.read('home/local-b/profile.id')
        except StopIteration: pass
        else: raise RuntimeError('destination already contains the imported profile')
        initial_transport = check_allocation(disks['transport'], out, 'transport-initial')
        try: initial_transport.read('transfer.json')
        except StopIteration: pass
        else: raise RuntimeError('transport already contains an export')
        d.save_json(out / 'receipt.json', receipt)
        boot(disks['a'], disks['transport'], out, 'a-export', args, 'export')
        exported = check_allocation(disks['transport'], out, 'transport-export').read('transfer.json')
        (out / 'guest-export.json').write_bytes(exported)
        bundle = profile_bundle.decode_json(exported)
        payloads = profile_bundle.validate_bundle(bundle)
        if (bundle['profile']['id'] != d.ID or payloads.get('Documents/migration.txt') != d.EDITED or
            payloads.get('Documents/binary.dat') != d.BINARY or
            bundle['preferences'] != dict(d.make_bundle()['preferences'], theme_style=1)):
            raise RuntimeError('guest export does not contain the edited profile')
        receipt['guest_export'] = {'path': str(out / 'guest-export.json'), 'sha256': d.digest(exported),
                                   'size': len(exported), 'profile_id': d.ID}
        d.save_json(out / 'receipt.json', receipt)
        receipt['interruptions'] = []
        # Each interrupted import uses a separate copy of pristine B. This avoids
        # making any claim that duplicate IDs or bindings coexist on one system.
        for checkpoint in ('journal', 'directories', 'document', 'identity', 'commit-ready'):
            probe_disk = out / ('probe-' + checkpoint + '.img')
            shutil.copyfile(disks['b'], probe_disk)
            original_hash = d.digest(probe_disk.read_bytes())
            boot(probe_disk, disks['transport'], out, 'stop-' + checkpoint, args, 'interrupt', checkpoint)
            boot(probe_disk, disks['transport'], out, 'recover-' + checkpoint, args, 'recover', checkpoint)
            _, (offset, size) = d.partitions(probe_disk)
            if d.outside_data(probe_disk, offset, size) != bounds['b']:
                raise RuntimeError(f'{checkpoint}: protected disk bytes changed')
            probe = check_allocation(probe_disk, out, 'recovered-' + checkpoint)
            prefix = B.removeprefix('/fat/') + '/'
            if (probe.read(prefix + 'profile.id') != b'srvros-profile-v1\n' + d.ID.encode() + b'\n' or
                probe.read(prefix + 'Documents/migration.txt') != d.EDITED or
                probe.read(prefix + 'Documents/binary.dat') != d.BINARY or
                profile_bundle.decode_json(probe.read(prefix + profile_bundle.PREF_PATH)) != bundle['preferences'] or
                profile_bundle.decode_json(probe.read(prefix + 'profile-local.json')) != {'alias': 'local_b', 'uid': 2050, 'gid': 2051} or
                probe.read('home/desktop.conf') != machine_b):
                raise RuntimeError(f'{checkpoint}: recovered profile mismatch')
            try: probe.read(prefix + '.profile-import-pending')
            except StopIteration: pass
            else: raise RuntimeError(f'{checkpoint}: pending marker remains after recovery')
            receipt['interruptions'].append({'checkpoint': checkpoint, 'stop_status': 86,
                'rebooted_before_refusal_and_recovery': True, 'disk': str(probe_disk),
                'initial_disk_sha256': original_hash, 'final_disk_sha256': d.digest(probe_disk.read_bytes())})
            d.save_json(out / 'receipt.json', receipt)
        boot(disks['b'], disks['transport'], out, 'b-import', args, 'import')
        boot(disks['b'], disks['transport'], out, 'b-activate', args, 'activate')
        for name, disk in disks.items():
            _, (offset, size) = d.partitions(disk)
            if d.outside_data(disk, offset, size) != bounds[name]:
                raise RuntimeError(f'{name}: disk bytes outside data partition changed')
            receipt['installations'][name]['final_disk_sha256'] = d.digest(disk.read_bytes())
        final_transport = check_allocation(disks['transport'], out, 'transport-final')
        if final_transport.read('transfer.json') != exported: raise RuntimeError('transport bundle changed')
        for name, home, machine, binding in (
                ('a', A, machine_a, {'alias': 'local_a', 'uid': 1001, 'gid': 1001}),
                ('b', B, machine_b, {'alias': 'local_b', 'uid': 2050, 'gid': 2051})):
            fixture = check_allocation(disks[name], out, name + '-final')
            prefix = home.removeprefix('/fat/') + '/'
            if fixture.read(prefix + 'profile.id') != b'srvros-profile-v1\n' + d.ID.encode() + b'\n':
                raise RuntimeError(f'{name}: profile identity mismatch')
            if profile_bundle.decode_json(fixture.read(prefix + 'profile-local.json')) != binding:
                raise RuntimeError(f'{name}: local binding mismatch')
            if fixture.read(prefix + 'Documents/migration.txt') != d.EDITED or fixture.read(prefix + 'Documents/binary.dat') != d.BINARY:
                raise RuntimeError(f'{name}: document mismatch')
            portable = fixture.read(prefix + profile_bundle.PREF_PATH)
            if profile_bundle.decode_json(portable) != bundle['preferences']:
                raise RuntimeError(f'{name}: portable preferences mismatch')
            if fixture.read('home/desktop.conf') != machine: raise RuntimeError(f'{name}: machine settings changed')
            try: fixture.read(prefix + '.profile-import-pending')
            except StopIteration: pass
            else: raise RuntimeError(f'{name}: pending import remains')
            for binary in ('deskd', 'desktools', 'deskterm', 'profile'):
                expected = runtime_elf((ROOT / f'build/userspace/{binary}.elf').read_bytes())
                if fixture.read('bin/' + binary) != expected:
                    raise RuntimeError(f'{name}: installed {binary} differs from matched build')
            receipt['installations'][name]['profile'] = {'id': d.ID, 'home': home, 'binding': binding,
                'document_sha256': d.digest(d.EDITED), 'binary_sha256': d.digest(d.BINARY),
                'portable_sha256': d.digest(portable), 'machine_sha256': d.digest(machine)}
        receipt.update(status='PASS', limits=['disposable QEMU images only',
            'no authentication or access-control claim', 'no physical disk or power-loss acceptance',
            'external fsck.exfat not run'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        d.save_json(out / 'receipt.json', receipt)
    print(f'profile-guest: PASS; evidence {out}', flush=True)


if __name__ == '__main__': main()
