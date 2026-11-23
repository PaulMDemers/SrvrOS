#!/usr/bin/env python3
"""Qualify the Lua, Berkeley Yacc and MiniZip commands in a disposable guest."""
import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from earlier_port_cases import byacc_cases, lua_cases, zip_cases, verify_native
from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from patch_smoke import directory_snapshot, validate_metadata
from thread_smoke import has_fatal_exception

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ('lua', 'byacc', 'minizip', 'miniunz')
FIXTURE = 'earlier-cli'
FSCK = rb'exfat-check: mounts=1 files=\d+ clusters=\d+ leaks=0 fat=0 errors=0 ok'


def digest(data):
    return sha256(data).hexdigest()


def boot(disk, out, qemu, receipt):
    serial_output = bytearray()
    consumed = 0
    with tempfile.TemporaryDirectory(prefix='earlier-cli-vm-') as temp:
        scratch = Path(temp)
        iso = build_iso(str(ROOT), temp, 1280, 800, os.environ.copy())
        boot_dir = scratch/'iso_root_1280x800/boot'
        for name in ('srvros.elf', 'initramfs.tar'):
            if digest((boot_dir/name).read_bytes()) != receipt['boot_sha256'][name]:
                raise RuntimeError(f'Guest boot file differs from build: {name}')
        receipt['iso_sha256'] = digest(Path(iso).read_bytes())
        port = choose_port(30001, 38000)
        qmp_path = scratch/'qmp'
        command = [qemu, '-M', 'q35', '-m', '512M', '-boot', 'd', '-cdrom', str(iso),
                   '-drive', f'if=none,id=testdisk,file={disk},format=raw',
                   '-device', 'ich9-ahci,id=ahci', '-device', 'ide-hd,drive=testdisk,bus=ahci.0',
                   '-device', 'qemu-xhci,id=xhci', '-device', 'usb-kbd,bus=xhci.0',
                   '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off',
                   '-qmp', f'unix:{qmp_path},server=on,wait=off',
                   '-display', 'none', '-monitor', 'none', '-no-reboot']
        with (out/'qemu.log').open('wb') as log:
            process = subprocess.Popen(command, stdout=log, stderr=log)
            serial = qmp = None
            try:
                serial = connect_serial(port, 25)
                serial.settimeout(0.2)
                qmp = QMP(qmp_path)

                def wait(marker, timeout):
                    nonlocal consumed
                    start = time.monotonic()
                    while marker not in serial_output[consumed:]:
                        elapsed = time.monotonic() - start
                        if not consumed:
                            qmp.boot_if_menu_stalled(serial_output, elapsed)
                        if elapsed > timeout or process.poll() is not None:
                            raise RuntimeError(f'Guest did not reach {marker!r}')
                        try:
                            chunk = serial.recv(16384)
                            if not chunk:
                                raise RuntimeError('Guest serial disconnected')
                            serial_output.extend(chunk)
                        except socket.timeout:
                            continue
                    consumed = serial_output.index(marker, consumed) + len(marker)

                wait(b'srv> ', 130)
                serial.sendall(f'run /fat/bin/sh /fat/{FIXTURE}/workflow.sh\n'.encode())
                wait(b'EARLIER_CLI_WORKFLOW_DONE', 220)
                wait(b'srv> ', 40)
                serial.sendall(b'fsck /fat\n')
                wait(b'exfat-check:', 40)
                wait(b'srv> ', 40)
            finally:
                if serial:
                    serial.close()
                if qmp:
                    qmp.stream.close()
                    qmp.sock.close()
                stop_process(process)
                (out/'serial.log').write_bytes(serial_output)
    if not re.search(FSCK, serial_output) or has_fatal_exception(serial_output.decode(errors='replace')):
        raise RuntimeError('Guest filesystem or process check failed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default=os.environ.get('QEMU', 'qemu-system-x86_64'))
    parser.add_argument('--disk', type=Path, default=ROOT/'build/srvros.exfat')
    parser.add_argument('--native-lua', required=True, type=Path)
    parser.add_argument('--native-byacc', required=True, type=Path)
    args = parser.parse_args()
    base = ROOT/'build/review/earlier-cli'
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    receipt = {'status': 'INCOMPLETE'}
    print('Earlier CLI evidence:', out, flush=True)
    try:
        fixtures = {}
        commands = ['#!/fat/bin/sh']
        expected = {}
        diagnostics = []
        for prepare in (lua_cases, byacc_cases, zip_cases):
            files, lines, checks, nonempty = prepare()
            if set(files) & set(fixtures) or set(checks) & set(expected):
                raise ValueError('Duplicate fixture or expected output')
            fixtures.update(files)
            commands.extend(lines)
            expected.update(checks)
            diagnostics.extend(nonempty)
        guest = f'/fat/{FIXTURE}'
        probe = (ROOT/'build/userspace/test_earlier_lua_io.elf').read_bytes()
        fixtures['lua-io-probe'] = runtime_elf(probe)
        receipt['probe_elf_sha256'] = digest(probe)
        commands.extend((f'chmod 755 {guest}/lua-io-probe',
                         f'{guest}/lua-io-probe > {guest}/lua-io.out 2> {guest}/lua-io.err',
                         f'echo $? > {guest}/lua-io.status'))
        expected['lua-io.status'] = b'0\n'
        expected['lua-io.out'] = (b'lua-io: read-only stdout refused\n'
                                  b'lua-io: incomplete stdin refused with preloaded input\n')
        diagnostics.append('lua-io.err')
        commands.append('echo EARLIER_CLI_WORKFLOW_DONE')
        fixtures['workflow.sh'] = ('\n'.join(commands) + '\n').encode()
        data = args.disk.read_bytes()
        image = ExfatFixture(data)
        baseline = directory_snapshot(image)
        if FIXTURE in baseline['root']:
            raise RuntimeError('Base disk already contains test fixtures')
        receipt['source_disk_sha256'] = digest(data)
        receipt['commands'] = list(TOOLS)
        receipt['artifacts'] = {}
        for tool in TOOLS:
            binary = (ROOT/f'build/userspace/{tool}.elf').read_bytes()
            staged = image.read(f'bin/{tool}')
            if runtime_elf(binary) != staged:
                raise RuntimeError(f'Staged {tool} differs from the build')
            receipt['artifacts'][tool] = {'elf': digest(binary), 'staged': digest(staged)}
        receipt['boot_sha256'] = {name: digest((ROOT/f'build/iso_root/boot/{name}').read_bytes())
                                  for name in ('srvros.elf', 'initramfs.tar')}
        receipt['fixtures_sha256'] = {name: digest(value) for name, value in fixtures.items()}
        receipt['head'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        receipt['build_manifest_sha256'] = digest((ROOT/'build/macos-build.json').read_bytes())
        image.add_directory_files(FIXTURE, fixtures)
        disk = out/'disk.exfat'
        disk.write_bytes(image.data)
        boot(disk, out, args.qemu, receipt)
        mounted = ExfatFixture(disk.read_bytes())
        observed = directory_snapshot(mounted)
        if (set(observed['root']) != set(baseline['root']) | {FIXTURE, '.srvros'} or
                observed['home'] != baseline['home'] or observed['tmp'] != baseline['tmp'] or
                set(observed['sidecar']) != {'meta'} or observed['sidecar']['meta']['attributes'] & 0x10):
            raise RuntimeError('Unexpected change outside the fixture')
        receipt['metadata_records'] = validate_metadata(mounted.read('.srvros/meta'))
        receipt['observed_sha256'] = {}

        def read_guest(name):
            content = mounted.read(f'{FIXTURE}/{name}')
            dest = out/'guest'/name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(content)
            receipt['observed_sha256'][name] = digest(content)
            return content

        for name, contents in {**fixtures, **expected}.items():
            actual = read_guest(name)
            if actual != contents:
                raise RuntimeError(f'{name}: expected {digest(contents)}, observed {digest(actual)}')
        for name in diagnostics:
            if not read_guest(name).strip():
                raise RuntimeError(f'{name}: no diagnostic')
        receipt['host_checks'] = verify_native(args, fixtures, read_guest, out)
        receipt['filesystem'] = 'clean'
        receipt['status'] = 'PASS'
        print('Earlier CLI smoke: PASS', out, flush=True)
    except Exception as error:
        receipt['status'] = 'FAIL'
        receipt['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        (out/'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')


if __name__ == '__main__':
    main()
