#!/usr/bin/env python3
"""Link and execute the native GLib prerequisites in a disposable SrvrOS guest."""

import hashlib
import io
import json
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tarfile
import tempfile
import time

from browser_runtime_smoke import receive, receive_run
from browser_storage import StorageGuard
from pack_runtime_elf import runtime_elf
from signal_public_guest import sdk_headers

ROOT = Path(__file__).resolve().parents[1]
MARKER = b'glib-platform: ok'
GUI_MARKER = b'gui-wait-capture: PASS deadlines, delayed pipe, thread cancellation, descriptor replacement, simultaneous GUI/pipe and peer exit'
FAULT = re.compile(rb'exception:|kernel panic|page fault|general protection fault', re.I)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run_guest(image, output, receipt):
    with tempfile.TemporaryDirectory(prefix='srv-glib-', dir='/tmp') as temp:
        uart = str(Path(temp) / 'serial')
        argv = ['qemu-system-x86_64', '-name', 'SrvrOS GLib prerequisite probe',
                '-machine', 'q35', '-accel', 'tcg,thread=single', '-cpu', 'qemu64',
                '-smp', '1', '-m', '512M', '-cdrom', str(image), '-boot', 'd',
                '-display', 'none', '-nic', 'none', '-serial',
                f'unix:{uart},server=on,wait=off', '-monitor', 'none', '-no-reboot']
        receipt['qemu_argv'] = argv
        with (output / 'qemu.log').open('wb') as log, (output / 'serial.log').open('wb') as serial:
            process = subprocess.Popen(argv, stdout=log, stderr=log)
            try:
                with socket.socket(socket.AF_UNIX) as connection:
                    deadline = time.monotonic() + 10
                    while True:
                        try:
                            connection.connect(uart)
                            break
                        except (FileNotFoundError, ConnectionRefusedError):
                            if process.poll() is not None or time.monotonic() >= deadline:
                                raise RuntimeError('guest serial connection failed')
                            time.sleep(0.05)
                    connection.settimeout(0.2)
                    boot = receive(connection, b'srv> ', 30)
                    serial.write(boot)
                    if b'srv> ' not in boot or FAULT.search(boot):
                        raise RuntimeError('guest did not reach the monitor')
                    receipt['tests'] = []
                    for attempt in (1, 2):
                        connection.sendall(b'run /glib-platform\n')
                        result = receive_run(connection, 15)
                        serial.write(result)
                        exits = list(re.finditer(rb'(?m)^run: pid=\d+ exited status=(\d+)\r?\n', result))
                        if len(exits) != 1 or FAULT.search(result):
                            raise RuntimeError('unexpected guest exit receipts or fault')
                        status = exits[0]
                        if status[1] != b'0' or b'srv> ' not in result[status.end():]:
                            raise RuntimeError('guest probe did not exit successfully and return to monitor')
                        for marker in (MARKER, GUI_MARKER):
                            matches = re.findall(rb'(?m)^' + re.escape(marker) + rb'\r?$', result[:status.start()])
                            if len(matches) != 1:
                                raise RuntimeError(f'missing or duplicate guest marker: {marker.decode()}')
                        receipt['tests'].append({'attempt': attempt, 'exit_status': 0, 'returned_to_monitor': True})
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
                receipt['guest_stopped'] = process.poll() is not None


def main():
    base = ROOT / 'build/browser-glib-prereq'
    base.mkdir(parents=True, exist_ok=True)
    StorageGuard(base).preflight()
    output = Path(tempfile.mkdtemp(prefix='guest-', dir=base))
    print(f'GLib prerequisite evidence: {output}', flush=True)
    receipt = {'result': 'failed', 'guest_stopped': False,
               'scope': 'native SrvrOS prerequisites in a normal diskless guest'}
    try:
        compiler = ROOT / 'ports/srvros/webkit/cc'
        source = ROOT / 'tools/test_glib_platform_guest.c'
        sdk = ROOT / 'build/sysroot/srvros'
        limine = ROOT / 'build/limine-binary'
        kernel = ROOT / 'build/kernel/srvros.elf'
        init = ROOT / 'build/userspace/init.elf'
        gui_source = ROOT / 'userspace/guifail/wait_capture.c'
        provenance = [Path(__file__), source, gui_source, compiler, compiler.with_name('compiler.py'),
                      ROOT / 'tools/pack_runtime_elf.py', ROOT / 'tools/browser_runtime_smoke.py',
                      ROOT / 'tools/signal_public_guest.py', ROOT / 'build/tooling/zig/zig', kernel, init,
                      sdk / 'lib/libsrvros.a', sdk / 'lib/crt0.o', sdk / 'lib/app.ld', limine / 'limine',
                      limine / 'limine-bios.sys', limine / 'limine-bios-cd.bin']
        receipt['source'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        receipt['sdk_headers_sha256'] = sdk_headers(sdk)
        receipt['inputs_sha256'] = {str(path.relative_to(ROOT)): digest(path) for path in provenance}
        objects = [output / 'platform.o', output / 'wait-capture.o']
        for unit, obj in zip((source, gui_source), objects):
            subprocess.run([compiler, '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-c', unit, '-o', obj], cwd=ROOT, check=True, timeout=60)
        elf = output / 'platform.elf'
        subprocess.run([compiler, *objects, '-o', elf], cwd=ROOT, check=True, timeout=60)
        receipt['payload_sha256'] = digest(elf)
        boot = output / 'stage/boot/limine'
        boot.mkdir(parents=True)
        shutil.copyfile(kernel, boot.parent / 'srvros.elf')
        for name in ('limine-bios.sys', 'limine-bios-cd.bin'):
            shutil.copyfile(limine / name, boot / name)
        (boot / 'limine.conf').write_text(
            'timeout: 0\nserial: yes\n\n/glib-platform\n'
            '    protocol: limine\n    path: boot():/boot/srvros.elf\n'
            '    cmdline: srvros.log=quiet\n'
            '    module_path: boot():/boot/initramfs.tar\n    module_string: initramfs\n')
        with tarfile.open(boot.parent / 'initramfs.tar', 'w', format=tarfile.USTAR_FORMAT) as archive:
            for name, path in [('init', init), ('glib-platform', elf)]:
                data = runtime_elf(path.read_bytes())
                entry = tarfile.TarInfo(name)
                entry.mode, entry.size = 0o755, len(data)
                archive.addfile(entry, io.BytesIO(data))
        image = output / 'guest.iso'
        with (output / 'image.log').open('wb') as log:
            subprocess.run(['xorriso', '-as', 'mkisofs', '-R', '-r', '-J', '-b',
                            'boot/limine/limine-bios-cd.bin', '-no-emul-boot', '-boot-load-size',
                            '4', '-boot-info-table', output / 'stage', '-o', image],
                           check=True, timeout=60, stdout=log, stderr=log)
            subprocess.run([limine / 'limine', 'bios-install', image], check=True,
                           timeout=30, stdout=log, stderr=log)
        receipt['image_sha256'] = digest(image)
        run_guest(image, output, receipt)
        receipt['result'] = 'passed'
    except Exception as error:
        receipt['error'] = str(error)
        raise
    finally:
        if (output / 'serial.log').is_file():
            receipt['serial_sha256'] = digest(output / 'serial.log')
        (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('GLib platform guest passed')


if __name__ == '__main__':
    main()
