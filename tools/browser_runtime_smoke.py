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
from browser_evidence import verify_runtime, verify_icu

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
    parser.add_argument('--icu', action='store_true', help='also verify and exercise the pinned ICU guest twice')
    parser.add_argument('--runtime-extensions', action='store_true', help='also exercise math, C++ linkage and algorithm providers')
    parser.add_argument('--signal-contract', action='store_true', help='also check the existing signal boundary')
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
    if args.icu:
        artifacts['icuprobe'] = ROOT / 'build/browser-icu/icuprobe.elf'
        artifacts['rttiprobe'] = ROOT / 'build/browser-runtime/rttiprobe.elf'
        artifacts['mathprobe'] = ROOT / 'build/browser-runtime/mathprobe.elf'
    if args.runtime_extensions:
        artifacts.update({'libmprobe': ROOT / 'build/browser-libm/libmprobe.elf',
                          'cpp-linkage': ROOT / 'build/browser-runtime/cpp-linkage.elf',
                          'algorithmprobe': ROOT / 'build/browser-runtime/algorithmprobe.elf'})
    if args.signal_contract:
        artifacts['signal-contract'] = ROOT / 'build/browser-runtime/signal-contract.elf'
        artifacts['signal-source'] = ROOT / 'tools/test_browser_signal_contract.c'
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
               'harness_sha256': {name: digest(ROOT / 'tools' / name) for name in
                                  ('browser_runtime_smoke.py', 'browser_evidence.py', 'pack_runtime_elf.py')},
               'tests': []}
    output = bytearray()
    try:
        receipt['runtime_provenance'] = verify_runtime()
        if args.icu:
            receipt['icu_provenance'] = verify_icu()
        if args.runtime_extensions:
            extension_receipt = ROOT / 'build/browser-runtime/extension-receipt.json'
            data = json.loads(extension_receipt.read_text()); hashes = data['sha256']
            if data.get('result') != 'success' or data.get('sdk_headers_sha256') != receipt['runtime_provenance']['sdk_headers_sha256']:
                raise ValueError('extension receipt missing success or mismatched SDK')
            required = ('tools/browser_extension_probe.py', 'tools/test_browser_cpp_linkage.cc',
                        'ports/srvros/libcxx/algorithm_guest.cc', 'build/browser-libm/libmprobe.elf',
                        'build/browser-runtime/cpp-linkage.elf', 'build/browser-runtime/algorithmprobe.elf',
                        'build/browser-filesystem/receipt.json', 'build/browser-libm/receipt.json')
            if not all(name in hashes for name in required): raise ValueError('extension receipt missing required input')
            for name, expected in hashes.items():
                path = Path(name)
                if path.is_absolute() or '..' in path.parts or not (ROOT/path).is_file() or digest(ROOT/path) != expected:
                    raise ValueError('extension receipt mismatch: ' + name)
            receipt['extension_provenance'] = {'path': str(extension_receipt), 'sha256': digest(extension_receipt)}
        if args.signal_contract:
            signal_receipt = ROOT / 'build/browser-runtime/signal-receipt.json'
            signal_data = json.loads(signal_receipt.read_text())
            if signal_data.get('result') != 'success' or signal_data.get('sdk_headers_sha256') != receipt['runtime_provenance']['sdk_headers_sha256']:
                raise ValueError('signal build receipt missing success or mismatched SDK')
            required = ('tools/browser_signal_probe.py', 'tools/test_browser_signal_contract.c',
                        'build/browser-runtime/signal-contract.elf', 'build/sysroot/srvros/lib/libsrvros.a',
                        'ports/srvros/webkit/compiler.py', 'ports/srvros/webkit/cc')
            hashes = signal_data['sha256']
            if not all(name in hashes for name in required):
                raise ValueError('signal build receipt missing required inputs')
            for name, expected in hashes.items():
                path = Path(name)
                if path.is_absolute() or '..' in path.parts or not (ROOT / path).is_file() or digest(ROOT / path) != expected:
                    raise ValueError('signal build receipt mismatch: ' + name)
            receipt['signal_provenance'] = {'path': str(signal_receipt), 'sha256': digest(signal_receipt)}
        if digest(artifacts['test']) != digest(ROOT / 'build/browser-runtime/cxxthreads.elf'):
            raise ValueError('test ELF differs from verified runtime build')
        iso_root = run / 'iso-root'
        boot = iso_root / 'boot/limine'
        boot.mkdir(parents=True)
        shutil.copyfile(artifacts['kernel'], iso_root / 'boot/srvros.elf')
        shutil.copyfile(artifacts['limine_bios'], boot / 'limine-bios.sys')
        shutil.copyfile(artifacts['limine_cd'], boot / 'limine-bios-cd.bin')
        with tarfile.open(iso_root / 'boot/initramfs.tar', 'w', format=tarfile.USTAR_FORMAT) as archive:
            programs = [('init', 'init'), ('cxxthreads', 'test'), ('tlsfirst', 'tlsfirst'),
                        ('cxxprobe', 'cxxprobe'), ('cxxstlprobe', 'cxxstlprobe')]
            if args.icu:
                programs.extend([('icuprobe', 'icuprobe'), ('rttiprobe', 'rttiprobe'),
                                 ('mathprobe', 'mathprobe')])
            if args.runtime_extensions:
                programs.extend((name, name) for name in ('libmprobe', 'cpp-linkage', 'algorithmprobe'))
            if args.signal_contract:
                programs.append(('signal-contract', 'signal-contract'))
            for name, key in programs:
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
                        tests = [('cxxprobe', 0), ('cxxstlprobe', 0), ('tlsfirst', 0),
                                 ('all', 0), ('exhaust', 134), ('all', 0)]
                        if args.icu:
                            tests = [('mathprobe', 0), ('rttiprobe', 0), ('badcast', 134), ('icuprobe', 0),
                                     *tests, ('icuprobe', 0)]
                        if args.runtime_extensions:
                            tests.extend((name, 0) for name in ('libmprobe', 'cpp-linkage', 'algorithmprobe'))
                        if args.signal_contract:
                            tests.append(('signal-contract', 0))
                        for mode, status in tests:
                            start = time.monotonic()
                            command = (f'run /{mode}' if mode in ('tlsfirst', 'cxxprobe', 'cxxstlprobe', 'icuprobe', 'rttiprobe', 'mathprobe', 'signal-contract', 'libmprobe', 'cpp-linkage', 'algorithmprobe')
                                       else 'run /rttiprobe badcast' if mode == 'badcast'
                                       else f'run /cxxthreads {mode}')
                            sock.sendall((command + '\n').encode())
                            result = receive_run(sock, 90 if mode == 'icuprobe' else 45)
                            output.extend(result)
                            exits = re.findall(rb'run: pid=\d+ exited status=(\d+)', result)
                            marker = (b'libmprobe: ok' if mode == 'libmprobe' else
                                      b'browser-c-linkage: ok' if mode == 'cpp-linkage' else
                                      b'browser-algorithm: ok' if mode == 'algorithmprobe' else
                                      b'browser-signal-contract: siginfo rejected; handler preserved' if mode == 'signal-contract' else
                                      f'{mode}: ok'.encode() if mode in ('cxxprobe', 'cxxstlprobe', 'icuprobe', 'rttiprobe', 'mathprobe') else
                                      b'rttiprobe: badcast must abort' if mode == 'badcast' else
                                      b'cxxthreads: ok' if mode == 'all' else
                                      b'cxxthreads: tls-first registered' if mode == 'tlsfirst' else
                                      b'cxxthreads: capacity=8; next construction must abort')
                            ok = (marker in result and exits == [str(status).encode()] and
                                  b'srv> ' in result and b'exception:' not in result)
                            if mode in ('cxxprobe', 'cxxstlprobe', 'icuprobe', 'rttiprobe', 'mathprobe', 'signal-contract', 'libmprobe', 'cpp-linkage', 'algorithmprobe'):
                                ok = ok and result.count(marker) == 1
                            elif mode == 'badcast':
                                ok = (ok and result.count(marker) == 1 and b'rttiprobe: ok' not in result
                                      and b'rttiprobe: unexpected success' not in result)
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
