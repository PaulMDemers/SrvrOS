#!/usr/bin/env python3
"""Run GLib platform probes in a disposable SrvrOS guest."""

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
FILE_POLICY_MARKER = b'glib-file-policy: ok'
CHILD_WATCH_MARKER = b'glib-child-watch: ok'
LINKED_CHILD_WATCH_MARKER = b'glib-linked-child-watch: ok'
FNMATCH_MARKER = b'glib-fnmatch: ok'
INET_ADDRESS_MARKER = b'glib-inet-address: ok'
INET_ATON_MARKER = b'glib-inet-aton: ok'
SEQPACKET_MARKER = b'glib-seqpacket: ok'
EPROTOTYPE_MARKER = b'glib-eprototype: ok'
SOMAXCONN_MARKER = b'glib-somaxconn: ok'
IP_TTL_MARKER = b'glib-ip-ttl: ok'
IPV6_UNICAST_HOPS_MARKER = b'glib-ipv6-unicast-hops: ok'
SO_BROADCAST_MARKER = b'glib-so-broadcast: ok'
IP_MULTICAST_LOOP_MARKER = b'glib-ip-multicast-loop: ok'
FIONREAD_MARKER = b'glib-fionread: ok'
GIO_POLL_MONITOR_MARKER = b'gio-poll-monitor: ok'
GIO_IP_TOS_MARKER = b'gio-ip-tos: ok'
GIO_IPV6_TCLASS_MARKER = b'gio-ipv6-tclass: ok'
GIO_RESOLVER_ADDRESS_MARKER = b'gio-resolver-address: ok'
FAULT = re.compile(rb'exception:|kernel panic|page fault|general protection fault', re.I)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run_guest(image, output, receipt, probes, dns_peer=None, workspace=None,
              probe_timeouts=None, *, idle_nic=False, ttl_peer=None, broadcast_peer=None):
    with tempfile.TemporaryDirectory(prefix='srv-glib-', dir='/tmp') as temp, ExitStack() as stack:
        uart = str(Path(temp) / 'serial')
        argv = ['qemu-system-x86_64', '-name', 'SrvrOS GLib prerequisite probe',
                '-machine', 'q35', '-accel', 'tcg,thread=single', '-cpu', 'qemu64',
                '-smp', '1', '-m', '512M', '-cdrom', str(image), '-boot', 'd',
                '-display', 'none', '-serial',
                f'unix:{uart},server=on,wait=off', '-monitor', 'none', '-no-reboot']
        pass_fds = ()
        wire_peer = dns_peer or ttl_peer or broadcast_peer
        capture_kind = 'ipv4_ttl' if ttl_peer else 'ipv4_broadcast' if broadcast_peer else None
        if wire_peer or idle_nic:
            a, b = socket.socketpair()
            stack.enter_context(a)
            stack.enter_context(b)
            if wire_peer:
                peer_kind = 'dns_peer' if dns_peer else f'{capture_kind}_peer'
                stats = output / (peer_kind.replace('_', '-') + '.json')
                peer_argv = [sys.executable, wire_peer, '--fd', str(a.fileno()), '--stats', stats]
                peer = stack.enter_context(subprocess.Popen(peer_argv, pass_fds=(a.fileno(),)))
                stack.callback(peer.terminate)
                a.close()
            pass_fds = (b.fileno(),)
            argv += ['-netdev', f'socket,id=fixture,fd={b.fileno()}', '-device', 'e1000,netdev=fixture']
        else:
            argv += ['-nic', 'none']
        if capture_kind:
            capture = output / (capture_kind.replace('_', '-') + '.pcap')
            capture_id = 'ttl-capture' if ttl_peer else 'broadcast-capture'
            argv += ['-object', f'filter-dump,id={capture_id},netdev=fixture,file={capture}']
        if workspace:
            argv += ['-drive', f'if=none,id=workspace,file={workspace},format=raw',
                     '-device', 'ich9-ahci,id=workspace-ahci',
                     '-device', 'ide-hd,drive=workspace,bus=workspace-ahci.0']
        receipt['qemu_argv'] = argv
        with (output / 'qemu.log').open('wb') as log, (output / 'serial.log').open('wb') as serial:
            try:
                process = subprocess.Popen(argv, stdout=log, stderr=log, pass_fds=pass_fds)
            finally:
                if pass_fds:
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
                            timeout = probe_timeouts.get(name, 15) if probe_timeouts else 15
                            result = receive_run(connection, timeout)
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
                if wire_peer:
                    try:
                        peer_exit = peer.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        peer.kill()
                        peer_exit = peer.wait(timeout=5)
                    receipt[f'{peer_kind}_exit'] = peer_exit
                    if stats.is_file():
                        receipt[f'{peer_kind}_sha256'] = digest(stats)
                if capture_kind and capture.is_file():
                    receipt[f'{capture_kind}_capture_sha256'] = digest(capture)
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
        if capture_kind:
            observed = json.loads(stats.read_text()) if stats.is_file() else {}
            if peer_exit or observed.get('result') != 'passed':
                raise RuntimeError(f'{peer_kind} wire probe failed: {observed}')
            receipt[f'{capture_kind}_wire'] = observed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iconv-prefix', type=Path, help='target prefix from browser_glib_iconv.py')
    parser.add_argument('--pcre2-prefix', type=Path, help='target PCRE2 prefix from browser_glib_iconv.py')
    parser.add_argument('--libffi-prefix', type=Path, help='target libffi prefix from browser_glib_iconv.py')
    parser.add_argument('--zlib-prefix', type=Path, help='target zlib prefix from browser_glib_iconv.py')
    parser.add_argument('--proxy-intl-prefix', type=Path, help='target proxy-libintl prefix from browser_glib_iconv.py')
    parser.add_argument('--resolver', action='store_true', help='test the SDK resolver against a private DNS peer')
    parser.add_argument('--file-policy', action='store_true', help='test no-follow opens in initramfs and private exFAT')
    parser.add_argument('--child-watch', action='store_true', help='test native SIGCHLD wake and targeted reaping')
    parser.add_argument('--linked-child-watch', type=Path, metavar='ELF',
                        help='boot a previously linked SrvrOS GLib child-watch ELF from this checkout')
    parser.add_argument('--fnmatch', action='store_true', help='test SDK filename pattern matching')
    parser.add_argument('--inet-address', action='store_true', help='test SDK address classifiers and numeric lookup')
    parser.add_argument('--inet-aton', action='store_true', help='test SDK IPv4 numbers-and-dots parsing')
    parser.add_argument('--seqpacket', action='store_true', help='test native Unix packet pairs and descriptor transfer')
    parser.add_argument('--eprototype', action='store_true', help='test the protocol/type errno and socket creation flags')
    parser.add_argument('--somaxconn', action='store_true', help='test the Unix listener backlog and its public ceiling')
    parser.add_argument('--ip-ttl', action='store_true', help='test per-socket IPv4 TTL against a private peer')
    parser.add_argument('--ipv6-unicast-hops', action='store_true', help='test IPv6 unicast option refusal without network')
    parser.add_argument('--so-broadcast', action='store_true', help='test IPv4 UDP broadcast against a private peer')
    parser.add_argument('--ip-multicast-loop', action='store_true', help='test explicit IPv4 multicast loop option refusal')
    parser.add_argument('--fionread', action='store_true', help='test non-consuming readable-byte queries')
    parser.add_argument('--gio-poll-monitor', type=Path, metavar='ELF',
                        help='boot a linked GIO monitor probe with a private exFAT disk')
    parser.add_argument('--gio-ip-tos', type=Path, metavar='ELF',
                        help='boot a linked GIO IPv4 traffic class codec probe')
    parser.add_argument('--gio-ipv6-tclass', type=Path, metavar='ELF',
                        help='boot a linked GIO IPv6 traffic class codec probe')
    parser.add_argument('--gio-resolver-address', type=Path, metavar='ELF',
                        help='boot a linked GIO resolver address handling probe')
    args = parser.parse_args()
    if sum((args.ip_ttl, args.resolver, args.so_broadcast)) > 1:
        parser.error('--ip-ttl, --resolver and --so-broadcast use separate private peers')
    base = ROOT / 'build/browser-glib-prereq'
    base.mkdir(parents=True, exist_ok=True)
    StorageGuard(base).preflight()
    output = Path(tempfile.mkdtemp(prefix='guest-', dir=base))
    print(f'GLib prerequisite evidence: {output}', flush=True)
    receipt = {'result': 'failed', 'guest_stopped': False,
               'scope': ('native SrvrOS prerequisites with a disposable exFAT disk'
                         if args.file_policy or args.gio_poll_monitor
                         else 'native SrvrOS prerequisites in a normal diskless guest')}
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
        workspace = None
        if args.child_watch:
            unit = ROOT / 'tools/test_glib_child_watch_guest.c'
            receipt['inputs_sha256'][str(unit.relative_to(ROOT))] = digest(unit)
            obj = output / 'child-watch.o'
            payload = output / 'child-watch.elf'
            subprocess.run([compiler, '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-c', unit, '-o', obj], cwd=ROOT, check=True, timeout=60)
            subprocess.run([compiler, obj, '-o', payload], cwd=ROOT, check=True, timeout=60)
            payloads['glib-child-watch'] = payload
            probes['glib-child-watch'] = (CHILD_WATCH_MARKER,)
            receipt['child_watch_payload_sha256'] = digest(payload)
        linked_probes = (
            ('glib-linked-child-watch', args.linked_child_watch, LINKED_CHILD_WATCH_MARKER,
             'linked_child_watch_payload_sha256'),
            ('gio-poll-monitor', args.gio_poll_monitor, GIO_POLL_MONITOR_MARKER,
             'gio_poll_monitor_payload_sha256'),
            ('gio-ip-tos', args.gio_ip_tos, GIO_IP_TOS_MARKER, 'gio_ip_tos_payload_sha256'),
            ('gio-ipv6-tclass', args.gio_ipv6_tclass, GIO_IPV6_TCLASS_MARKER,
             'gio_ipv6_tclass_payload_sha256'),
            ('gio-resolver-address', args.gio_resolver_address, GIO_RESOLVER_ADDRESS_MARKER,
             'gio_resolver_address_payload_sha256'),
        )
        for program, path, marker, receipt_key in linked_probes:
            if path is None:
                continue
            linked = path.resolve(strict=True)
            linked_input = str(linked.relative_to(ROOT))
            payload = output / f"{program.removeprefix('glib-')}.elf"
            receipt['scope'] += f' with the linked target {program} probe'
            receipt['inputs_sha256'][linked_input] = digest(linked)
            shutil.copyfile(linked, payload)
            payload_hash = digest(payload)
            if payload_hash != receipt['inputs_sha256'][linked_input]:
                raise RuntimeError(f'{program} payload changed during staging')
            payloads[program] = payload
            probes[program] = (marker,)
            receipt[receipt_key] = payload_hash
        sdk_probes = (('fnmatch', args.fnmatch, FNMATCH_MARKER),
                      ('inet-address', args.inet_address, INET_ADDRESS_MARKER),
                      ('inet-aton', args.inet_aton, INET_ATON_MARKER),
                      ('seqpacket', args.seqpacket, SEQPACKET_MARKER),
                      ('eprototype', args.eprototype, EPROTOTYPE_MARKER),
                      ('somaxconn', args.somaxconn, SOMAXCONN_MARKER),
                      ('ip-ttl', args.ip_ttl, IP_TTL_MARKER),
                      ('ipv6-unicast-hops', args.ipv6_unicast_hops, IPV6_UNICAST_HOPS_MARKER),
                      ('so-broadcast', args.so_broadcast, SO_BROADCAST_MARKER),
                      ('ip-multicast-loop', args.ip_multicast_loop, IP_MULTICAST_LOOP_MARKER),
                      ('fionread', args.fionread, FIONREAD_MARKER))
        for name, enabled, marker in sdk_probes:
            if not enabled:
                continue
            unit = ROOT / f"tools/test_glib_{name.replace('-', '_')}_guest.c"
            receipt['inputs_sha256'][str(unit.relative_to(ROOT))] = digest(unit)
            obj = output / f'{name}.o'
            payload = output / f'{name}.elf'
            subprocess.run([compiler, '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-c', unit, '-o', obj], cwd=ROOT, check=True, timeout=60)
            subprocess.run([compiler, obj, '-o', payload], cwd=ROOT, check=True, timeout=60)
            receipt[f"{name.replace('-', '_')}_payload_sha256"] = digest(payload)
            payloads[f'glib-{name}'] = payload
            probes[f'glib-{name}'] = (marker,)
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
        if args.file_policy:
            unit = ROOT / 'tools/test_glib_file_policy_guest.c'
            receipt['inputs_sha256'][str(unit.relative_to(ROOT))] = digest(unit)
            obj = output / 'file-policy.o'
            payload = output / 'file-policy.elf'
            subprocess.run([compiler, '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-c', unit, '-o', obj], cwd=ROOT, check=True, timeout=60)
            subprocess.run([compiler, obj, '-o', payload], cwd=ROOT, check=True, timeout=60)
            payloads['glib-file-policy'] = payload
            probes['glib-file-policy'] = (FILE_POLICY_MARKER,)
            receipt['file_policy_payload_sha256'] = digest(payload)
        if args.file_policy or args.gio_poll_monitor:
            generator = ROOT / 'tools/mk_exfat_image.py'
            for path in (generator, ROOT / 'tools/audio_fixture.py'):
                receipt['inputs_sha256'][str(path.relative_to(ROOT))] = digest(path)
            workspace = output / 'workspace.exfat'
            with (output / 'workspace.log').open('wb') as log:
                subprocess.run([sys.executable, generator, workspace], cwd=ROOT, check=True,
                               timeout=60, stdout=log, stderr=log)
            receipt['workspace_before_sha256'] = digest(workspace)
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
            if args.file_policy:
                data = b'sentinel\n'
                entry = tarfile.TarInfo('nofollow-target')
                entry.mode, entry.size = 0o644, len(data)
                archive.addfile(entry, io.BytesIO(data))
                entry = tarfile.TarInfo('nofollow-link')
                entry.type, entry.linkname = tarfile.SYMTYPE, 'nofollow-target'
                archive.addfile(entry)
        image = output / 'guest.iso'
        with (output / 'image.log').open('wb') as log:
            subprocess.run(['xorriso', '-as', 'mkisofs', '-R', '-r', '-J', '-b',
                            'boot/limine/limine-bios-cd.bin', '-no-emul-boot', '-boot-load-size',
                            '4', '-boot-info-table', output / 'stage', '-o', image],
                           check=True, timeout=60, stdout=log, stderr=log)
            subprocess.run([limine / 'limine', 'bios-install', image], check=True,
                           timeout=30, stdout=log, stderr=log)
        receipt['image_sha256'] = digest(image)
        ttl_peer = ROOT / 'tools/ipv4_ttl_test_peer.py' if args.ip_ttl else None
        broadcast_peer = ROOT / 'tools/ipv4_broadcast_test_peer.py' if args.so_broadcast else None
        idle_nic = args.gio_ip_tos is not None or args.ip_multicast_loop or args.fionread
        if ttl_peer:
            receipt['inputs_sha256'][str(ttl_peer.relative_to(ROOT))] = digest(ttl_peer)
            receipt['scope'] += ' and a private e1000 IPv4 TTL peer'
        elif broadcast_peer:
            for path in (broadcast_peer, ROOT / 'tools/ipv4_ttl_test_peer.py'):
                receipt['inputs_sha256'][str(path.relative_to(ROOT))] = digest(path)
            receipt['scope'] += ' and a private e1000 IPv4 broadcast peer'
        elif idle_nic and not dns_peer:
            receipt['scope'] += ' and a private e1000 on an idle local socketpair'
        probe_timeouts = {'gio-poll-monitor': 90, 'glib-ip-ttl': 30}
        run_guest(image, output, receipt, probes, dns_peer, workspace,
                  probe_timeouts, idle_nic=idle_nic,
                  ttl_peer=ttl_peer, broadcast_peer=broadcast_peer)
        receipt['result'] = 'passed'
    except Exception as error:
        receipt['error'] = str(error)
        raise
    finally:
        if (output / 'serial.log').is_file():
            receipt['serial_sha256'] = digest(output / 'serial.log')
        if (output / 'workspace.exfat').is_file():
            receipt['workspace_after_sha256'] = digest(output / 'workspace.exfat')
        (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('GLib platform guest passed')


if __name__ == '__main__':
    main()
