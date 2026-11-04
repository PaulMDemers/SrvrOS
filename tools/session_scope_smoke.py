#!/usr/bin/env python3
"""Bounded full-boot session-scope proof with the ordinary system-init reaper."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso, read_for
from vm_sockets import connect_unix


def main():
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/review/session-scope'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    manifest = json.loads((root / 'build/macos-build.json').read_text())
    payloads = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in (root / 'build/kernel/srvros.elf', root / 'build/initramfs.tar')}
    assert all(manifest['hashes'][p] == digest for p, digest in payloads.items())
    output = bytearray()
    checks = []
    # Large disposable ISO stays on the build volume; sockets use a short path.
    with tempfile.TemporaryDirectory(prefix='scope-iso-', dir=out) as iso_folder, \
            tempfile.TemporaryDirectory(prefix='scope-') as socket_folder:
        tmp = Path(socket_folder)
        iso = build_iso(str(root), iso_folder, 1024, 768, os.environ.copy(),
                        root / 'build/kernel/srvros.elf', root / 'build/initramfs.tar')
        cmd = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', iso,
               '-boot', 'd', '-nic', 'none', '-serial', f'unix:{tmp}/serial,server=on,wait=off',
               '-qmp', f'unix:{tmp}/qmp,server=on,wait=off', '-display', 'none',
               '-monitor', 'none', '-no-reboot']
        with (out / 'qemu.txt').open('w') as errors:
            process = subprocess.Popen(cmd, stdout=errors, stderr=errors)
            qmp = None
            try:
                serial = connect_unix(tmp / 'serial', 20)
                serial.settimeout(.05)
                qmp = QMP(tmp / 'qmp')
                start = time.monotonic()
                while b'\nsrv> ' not in output and time.monotonic() - start < 60:
                    output.extend(read_for(serial, .1))
                    qmp.boot_if_menu_stalled(output, time.monotonic() - start)
                assert b'\nsrv> ' in output, 'monitor unavailable'
                init = re.search(rb'init: started pid=(\d+)', output)
                assert init and b'init: startup complete' in output, 'system init not ready'

                def command(value, markers, timeout=120):
                    before = len(output)
                    serial.sendall(value.encode() + b'\n')
                    end = time.monotonic() + timeout
                    while b'\nsrv> ' not in output[before:] and time.monotonic() < end:
                        output.extend(read_for(serial, .2))
                    part = bytes(output[before:])
                    assert b'\nsrv> ' in part, f'{value}: did not return'
                    assert all(marker in part for marker in markers), f'{value}: missing expected output'
                    assert b'FAIL' not in part and b'exception:' not in part, f'{value}: failure'
                    checks.append(value)
                    return part

                def init_alive():
                    listing = command('ps', [])
                    assert re.search(rb'(?:^|\n)' + init[1] + rb' /init (?!exited)[^\r\n]+', listing), listing

                init_alive()
                for _ in range(2):
                    command('run /fat/bin/scopeprobe', [
                        b'scopeprobe: PASS failed root, held orphan, release and owner restart',
                        b'exited status=0'])
                    init_alive()
                command('lifetimetest', [
                    b'lifetime-test: PASS orphan exec setsid group-change descendants=2 unrelated=preserved drained=yes',
                    b'lifetime-test: failed root retired', b'lifetime-test: churn retired'])
                command('run /fat/bin/ptyprobe', [b'ptyprobe: PASS', b'exited status=0'])
                command('run /fat/bin/sysabi', [b'sysabi: ok', b'exited status=0'])
                init_alive()
                qmp.call('screendump', {'filename': str(out / 'screen.ppm')})
                result = {'status': 'PASS', 'build_source': manifest['source'],
                          'build_dirty': manifest['worktree_dirty'], 'payload_sha256': payloads,
                          'checks': checks, 'system_init_pid': int(init[1]),
                          'orphan_progress': 'Ownerless held descendant exits naturally; CREATE succeeds only after init retirement.',
                          'requires_live_system_init': True, 'physical_acceptance': False,
                          'qemu_command': cmd}
                (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
                print(json.dumps(result))
            finally:
                (out / 'serial.log').write_bytes(output)
                stop_process(process)


if __name__ == '__main__':
    main()
