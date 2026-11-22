#!/usr/bin/env python3
"""Run native GLib prerequisite probes in a disposable SrvrOS guest."""

import argparse
from contextlib import ExitStack
import hashlib
import io
import json
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
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
ICONV_MARKER = b'glib-iconv: ok'
PCRE2_MARKER = b'glib-pcre2: ok'
LIBFFI_MARKER = b'glib-libffi: ok'
ZLIB_MARKER = b'glib-zlib: ok'
PROXY_INTL_MARKER = b'glib-proxy-intl: ok'
RESOLVER_MARKER = b'glib-resolver: ok'
FAULT = re.compile(rb'exception:|kernel panic|page fault|general protection fault', re.I)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run_guest(image, output, receipt, probes, dns_peer=None):
    with tempfile.TemporaryDirectory(prefix='srv-glib-', dir='/tmp') as temp, ExitStack() as stack:
        uart = str(Path(temp) / 'serial')
        argv = ['qemu-system-x86_64', '-name', 'SrvrOS GLib prerequisite probe',
                '-machine', 'q35', '-accel', 'tcg,thread=single', '-cpu', 'qemu64',
                '-smp', '1', '-m', '512M', '-cdrom', str(image), '-boot', 'd',
                '-display', 'none', '-serial',
                f'unix:{uart},server=on,wait=off', '-monitor', 'none', '-no-reboot']
        pass_fds = ()
        if dns_peer:
            a, b = socket.socketpair()
            stack.enter_context(a)
            stack.enter_context(b)
            stats = output / 'dns-peer.json'
            peer_argv = [sys.executable, dns_peer, '--fd', str(a.fileno()), '--stats', stats]
            peer = stack.enter_context(subprocess.Popen(peer_argv, pass_fds=(a.fileno(),)))
            stack.callback(peer.terminate)
            a.close()
            pass_fds = (b.fileno(),)
            argv += ['-netdev', f'socket,id=fixture,fd={b.fileno()}', '-device', 'e1000,netdev=fixture']
        else:
            argv += ['-nic', 'none']
        receipt['qemu_argv'] = argv
        with (output / 'qemu.log').open('wb') as log, (output / 'serial.log').open('wb') as serial:
            try:
                process = subprocess.Popen(argv, stdout=log, stderr=log, pass_fds=pass_fds)
            finally:
                if dns_peer:
                    b.close()
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
                    for name, markers in probes.items():
                        for attempt in (1, 2):
                            connection.sendall(f'run /{name}\n'.encode())
                            result = receive_run(connection, 15)
                            serial.write(result)
                            exits = list(re.finditer(rb'(?m)^run: pid=\d+ exited status=(\d+)\r?\n', result))
                            if len(exits) != 1 or FAULT.search(result):
                                raise RuntimeError('unexpected guest exit receipts or fault')
                            status = exits[0]
                            if status[1] != b'0' or b'srv> ' not in result[status.end():]:
                                raise RuntimeError('guest probe did not exit successfully and return to monitor')
                            for marker in markers:
                                matches = re.findall(rb'(?m)^' + re.escape(marker) + rb'\r?$', result[:status.start()])
                                if len(matches) != 1:
                                    raise RuntimeError(f'missing or duplicate guest marker: {marker.decode()}')
                            receipt['tests'].append({'program': name, 'attempt': attempt,
                                                     'exit_status': 0, 'returned_to_monitor': True})
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
                receipt['guest_stopped'] = process.poll() is not None
                if dns_peer:
                    try:
                        peer_exit = peer.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        peer.kill()
                        peer_exit = peer.wait(timeout=5)
                    receipt['dns_peer_exit'] = peer_exit
                    if stats.is_file():
                        receipt['dns_peer_sha256'] = digest(stats)
        if dns_peer:
            expected = dict.fromkeys(('good', 'large', 'missing', 'nodata', 'temporary',
                                      'truncated', 'mismatch', 'legacy', 'malformed', 'service'), 2)
            expected.update(large=4, missing=4)
            observed = json.loads(stats.read_text())
            if (peer_exit or observed['queries'] != expected or observed['invalid_frames'] or
                observed['rejected_queries'] or observed['dns_responses'] != 30 or
                observed['deliberate_mismatch_replies'] != 4 or
                observed['deliberate_malformed_replies'] != 2):
                raise RuntimeError(f'unexpected DNS fixture traffic: {observed}')
            receipt['dns_peer_queries'] = observed['queries']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iconv-prefix', type=Path, help='target prefix from browser_glib_iconv.py')
    parser.add_argument('--pcre2-prefix', type=Path, help='target PCRE2 prefix from browser_glib_iconv.py')
    parser.add_argument('--libffi-prefix', type=Path, help='target libffi prefix from browser_glib_iconv.py')
    parser.add_argument('--zlib-prefix', type=Path, help='target zlib prefix from browser_glib_iconv.py')
    parser.add_argument('--proxy-intl-prefix', type=Path, help='target proxy-libintl prefix from browser_glib_iconv.py')
    parser.add_argument('--resolver', action='store_true', help='test the SDK resolver against a private DNS peer')
    args = parser.parse_args()
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
        payloads = {'glib-platform': elf}
        probes = {'glib-platform': (MARKER, GUI_MARKER)}
        dns_peer = None
        if args.resolver:
            dns_peer = ROOT / 'tools/resolver_dns_test_peer.py'
            unit = ROOT / 'tools/test_glib_resolver_guest.c'
            for path in (dns_peer, unit, ROOT / 'tools/test_glib_dns_expand.h'):
                receipt['inputs_sha256'][str(path.relative_to(ROOT))] = digest(path)
            obj = output / 'resolver.o'
            resolver = output / 'resolver.elf'
            subprocess.run([compiler, '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-c', unit, '-o', obj], cwd=ROOT, check=True, timeout=60)
            subprocess.run([compiler, obj, '-o', resolver], cwd=ROOT, check=True, timeout=60)
            payloads['glib-resolver'] = resolver
            probes['glib-resolver'] = (RESOLVER_MARKER,)
            receipt['resolver_payload_sha256'] = digest(resolver)
        dependencies = [
            ('iconv', args.iconv_prefix, ('iconv.h',), 'libiconv.a', ICONV_MARKER),
            ('pcre2', args.pcre2_prefix, ('pcre2.h',), 'libpcre2-8.a', PCRE2_MARKER),
            ('libffi', args.libffi_prefix, ('ffi.h', 'ffitarget.h'), 'libffi.a', LIBFFI_MARKER),
            ('zlib', args.zlib_prefix, ('zlib.h', 'zconf.h'), 'libz.a', ZLIB_MARKER),
            ('proxy-intl', args.proxy_intl_prefix, ('libintl.h',), 'libintl.a', PROXY_INTL_MARKER),
        ]
        for name, target_prefix, headers, archive_name, marker in dependencies:
            if target_prefix is None:
                continue
            prefix = target_prefix.resolve()
            unit = ROOT / f"tools/test_glib_{name.replace('-', '_')}_guest.c"
            include = prefix / 'include'
            library = prefix / 'lib' / archive_name
            receipt['inputs_sha256'][str(unit.relative_to(ROOT))] = digest(unit)
            receipt[f'{name}_provider_sha256'] = {str(path.relative_to(prefix)): digest(path)
                                                 for path in (library, *(include / header for header in headers))}
            obj = output / f'{name}.o'
            payload = output / f'{name}.elf'
            subprocess.run([compiler, '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-I', include, '-c', unit, '-o', obj],
                           cwd=ROOT, check=True, timeout=60)
            subprocess.run([compiler, obj, library, '-o', payload],
                           cwd=ROOT, check=True, timeout=60)
            receipt[f'{name}_payload_sha256'] = digest(payload)
            payloads[f'glib-{name}'] = payload
            probes[f'glib-{name}'] = (marker,)
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
            for name, path in [('init', init), *payloads.items()]:
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
        run_guest(image, output, receipt, probes, dns_peer)
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
