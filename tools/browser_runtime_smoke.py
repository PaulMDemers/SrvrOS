#!/usr/bin/env python3
"""Exercise the threaded C++ port in a disposable, diskless matched x86_64 guest."""
import argparse
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

from pack_runtime_elf import runtime_elf
from browser_evidence import verify_runtime

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def receive(sock, marker, seconds):
    data = bytearray()
    end = time.monotonic() + seconds
    while marker not in data and time.monotonic() < end:
        try:
            part = sock.recv(65536)
            if not part:
                break
            data.extend(part)
        except socket.timeout:
            pass
    return bytes(data)


def receive_run(sock, seconds):
    # The monitor redraws its prompt while echoing each input character. A
    # prompt is completion only after the command's newline-terminated receipt.
    data = bytearray()
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        try:
            part = sock.recv(65536)
            if not part:
                break
            data.extend(part)
            match = re.search(rb'run: pid=\d+ exited status=\d+\r?\n', data)
            if match and b'srv> ' in data[match.end():]:
                break
        except socket.timeout:
            pass
    return bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, default=ROOT / 'build/browser-runtime/cxxthreads.elf')
    args = parser.parse_args()
    artifacts = {
        'kernel': ROOT / 'build/kernel/srvros.elf',
        'init': ROOT / 'build/userspace/init.elf',
        'test': args.elf.resolve(),
        'tlsfirst': ROOT / 'build/browser-runtime/cxxthreads-tlsfirst.elf',
        'cxxprobe': ROOT / 'build/userspace/cxxprobe.elf',
        'cxxstlprobe': ROOT / 'build/userspace/cxxstlprobe.elf',
        'limine_bios': ROOT / 'build/limine-binary/limine-bios.sys',
        'limine_cd': ROOT / 'build/limine-binary/limine-bios-cd.bin',
    }
    for path in artifacts.values():
        if not path.is_file():
            parser.error(f'missing {path}; build the matched kernel/init, Limine and runtime test first')
    base = ROOT / 'build/browser-runtime'
    base.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='guest-', dir=base))
    receipt = {'scope': 'diskless Q35/TCG guest; no browser or hardware acceptance',
               'source_observation_only': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'dirty': subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
               'artifacts': {key: {'path': str(path), 'sha256': digest(path)} for key, path in artifacts.items()},
               'tests': []}
    output = bytearray()
    try:
        receipt['runtime_provenance'] = verify_runtime()
        if digest(artifacts['test']) != digest(ROOT / 'build/browser-runtime/cxxthreads.elf'):
            raise ValueError('test ELF differs from verified runtime build')
        iso_root = run / 'iso-root'
        boot = iso_root / 'boot/limine'
        boot.mkdir(parents=True)
        shutil.copyfile(artifacts['kernel'], iso_root / 'boot/srvros.elf')
        shutil.copyfile(artifacts['limine_bios'], boot / 'limine-bios.sys')
        shutil.copyfile(artifacts['limine_cd'], boot / 'limine-bios-cd.bin')
        with tarfile.open(iso_root / 'boot/initramfs.tar', 'w', format=tarfile.USTAR_FORMAT) as archive:
            for name, key in [('init', 'init'), ('cxxthreads', 'test'), ('tlsfirst', 'tlsfirst'),
                              ('cxxprobe', 'cxxprobe'), ('cxxstlprobe', 'cxxstlprobe')]:
                data = runtime_elf(artifacts[key].read_bytes())
                info = tarfile.TarInfo(name)
                info.size, info.mode, info.mtime = len(data), 0o755, 0
                archive.addfile(info, io.BytesIO(data))
        (boot / 'limine.conf').write_text('timeout: 0\nserial: yes\n\n/browser-runtime\n'
                                        '    protocol: limine\n    path: boot():/boot/srvros.elf\n'
                                        '    cmdline: srvros.log=quiet\n'
                                        '    module_path: boot():/boot/initramfs.tar\n'
                                        '    module_string: initramfs\n')
        iso = run / 'runtime.iso'
        with (run / 'image.log').open('wb') as log:
            subprocess.run(['xorriso', '-as', 'mkisofs', '-R', '-r', '-J', '-b',
                            'boot/limine/limine-bios-cd.bin', '-no-emul-boot', '-boot-load-size', '4',
                            '-boot-info-table', str(iso_root), '-o', str(iso)], check=True, timeout=60,
                           stdout=log, stderr=subprocess.STDOUT)
            subprocess.run([str(ROOT / 'build/limine-binary/limine'), 'bios-install', str(iso)],
                           check=True, timeout=30, stdout=log, stderr=subprocess.STDOUT)
        receipt['iso_sha256'] = digest(iso)
        with tempfile.TemporaryDirectory(prefix='srv-cxx-', dir='/tmp') as sockets:
            serial = str(Path(sockets) / 'serial')
            cmd = ['qemu-system-x86_64', '-name', 'SrvrOS C++ runtime probe', '-machine', 'q35',
                   '-accel', 'tcg,thread=single', '-cpu', 'qemu64', '-smp', '1', '-m', '512M',
                   '-cdrom', str(iso), '-boot', 'd', '-display', 'none', '-nic', 'none',
                   '-serial', f'unix:{serial},server=on,wait=off', '-monitor', 'none', '-no-reboot']
            receipt['qemu_command'] = cmd
            with (run / 'qemu.log').open('wb') as log:
                proc = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
                receipt['pid'] = proc.pid
                try:
                    with socket.socket(socket.AF_UNIX) as sock:
                        deadline = time.monotonic() + 15
                        while True:
                            try:
                                sock.connect(serial)
                                break
                            except (FileNotFoundError, ConnectionRefusedError):
                                if proc.poll() is not None or time.monotonic() >= deadline:
                                    raise RuntimeError('guest serial connection failed')
                                time.sleep(0.1)
                        sock.settimeout(0.2)
                        boot_log = receive(sock, b'srv> ', 60)
                        output.extend(boot_log)
                        if b'srv> ' not in boot_log:
                            raise RuntimeError('guest did not reach monitor')
                        for mode, status in [('cxxprobe', 0), ('cxxstlprobe', 0), ('tlsfirst', 0),
                                             ('all', 0), ('exhaust', 134), ('all', 0)]:
                            start = time.monotonic()
                            command = (f'run /{mode}' if mode in ('tlsfirst', 'cxxprobe', 'cxxstlprobe')
                                       else f'run /cxxthreads {mode}')
                            sock.sendall((command + '\n').encode())
                            result = receive_run(sock, 45)
                            output.extend(result)
                            exits = re.findall(rb'run: pid=\d+ exited status=(\d+)', result)
                            marker = (f'{mode}: ok'.encode() if mode in ('cxxprobe', 'cxxstlprobe') else
                                      b'cxxthreads: ok' if mode == 'all' else
                                      b'cxxthreads: tls-first registered' if mode == 'tlsfirst' else
                                      b'cxxthreads: capacity=8; next construction must abort')
                            ok = (marker in result and exits == [str(status).encode()] and
                                  b'srv> ' in result and b'exception:' not in result)
                            if mode in ('cxxprobe', 'cxxstlprobe'):
                                pass
                            elif mode == 'tlsfirst':
                                markers = (b'cxxthreads: tls-first registered', b'cxxthreads: tls-first tls',
                                           b'cxxthreads: tls-first global')
                                ok = ok and all(result.count(item) == 1 for item in markers)
                                ok = ok and result.find(markers[0]) < result.find(markers[1]) < result.find(markers[2])
                                ok = ok and b'cxxthreads: tls-first wrong order' not in result
                            elif mode == 'exhaust':
                                ok = ok and b'cxxthreads: ok' not in result
                            else:
                                ok = ok and all(result.count(item) == 1 for item in (
                                    b'cxxthreads: main tls destructor', b'cxxthreads: static destructor'))
                                ok = ok and (result.find(b'cxxthreads: main tls destructor') <
                                             result.find(b'cxxthreads: static destructor'))
                            receipt['tests'].append({'mode': mode, 'expected_status': status,
                                                     'seconds': round(time.monotonic() - start, 3), 'passed': ok})
                            if not ok:
                                raise RuntimeError(f'{mode}: missing marker, wrong exit or guest exception')
                finally:
                    proc.terminate()
                    try:
                        proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait(timeout=5)
                    receipt['guest_stopped'] = proc.poll() is not None
        receipt['result'] = 'passed'
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.SubprocessError) as error:
        receipt['result'], receipt['error'] = 'failed', str(error)
    finally:
        (run / 'serial.log').write_bytes(output)
        (run / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Receipt:', run / 'receipt.json')
    print('Result:', receipt['result'])
    return receipt['result'] != 'passed'


if __name__ == '__main__':
    raise SystemExit(main())
