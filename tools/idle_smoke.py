#!/usr/bin/env python3
"""Measure boot-CPU scheduler samples and host QEMU CPU at console/desktop idle.

Uses a disposable image. Host CPU figures are comparative observations, not
physical energy measurements. Optional ceilings make regression checks explicit.
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
import tempfile
import time
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=10, choices=range(5, 61), metavar='5..60')
    parser.add_argument('--output', type=Path, default=Path('build/review/idle'))
    parser.add_argument('--network', action='store_true')
    parser.add_argument('--max-host-cpu', type=float, help='Maximum percent of one host CPU in either idle phase')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    output = b''; consumed = 0; metrics = {'seconds': args.seconds, 'network': args.network,
        'source': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
        'worktree_dirty': bool(subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True).strip()),
        'kernel_sha256': hashlib.sha256((root/'build/kernel/srvros.elf').read_bytes()).hexdigest()}
    port = choose_port(30001, 38000)
    with tempfile.TemporaryDirectory(prefix='srvros-idle-') as directory:
        tmp = Path(directory)
        shutil.copyfile(root/'build/srvros.exfat', tmp/'disk.exfat')
        iso = build_iso(str(root), str(tmp), 1280, 800, os.environ.copy())
        metrics['boot_kernel_sha256']=hashlib.sha256((tmp/'iso_root_1280x800/boot/srvros.elf').read_bytes()).hexdigest()
        assert metrics['boot_kernel_sha256']==metrics['kernel_sha256'], 'Stale boot tree; run make all first'
        command = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', str(iso), '-boot', 'd',
            '-drive', f'if=none,id=disk,file={tmp/"disk.exfat"},format=raw',
            '-device', 'ich9-ahci,id=ahci', '-device', 'ide-hd,drive=disk,bus=ahci.0',
            '-device', 'qemu-xhci,id=xhci', '-device', 'usb-kbd,bus=xhci.0',
            '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off',
            '-qmp', f'unix:{tmp/"qmp"},server=on,wait=off', '-display', 'none', '-monitor', 'none', '-no-reboot']
        command += ['-netdev', 'user,id=net0', '-device', 'e1000,netdev=net0'] if args.network else ['-nic', 'none']
        with (out/'qemu.log').open('wb') as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                serial = connect_serial(port, 20); serial.settimeout(.1); qmp = QMP(tmp/'qmp')
                def receive():
                    nonlocal output
                    try:
                        chunk = serial.recv(16384)
                        if not chunk: raise RuntimeError('serial disconnected')
                        output += chunk
                    except socket.timeout: pass
                def wait(marker, timeout=40):
                    nonlocal consumed
                    start = time.monotonic()
                    while marker not in output[consumed:]:
                        if marker == b'srv> ' and not consumed:
                            qmp.boot_if_menu_stalled(output, time.monotonic()-start)
                        if time.monotonic()-start > timeout or process.poll() is not None:
                            raise RuntimeError(f'Missing {marker!r}; see {out}')
                        receive()
                    consumed = output.index(marker, consumed)+len(marker)
                def command_line(line):
                    nonlocal consumed
                    # Interactive line redraws also contain "srv> ". Only a
                    # fresh prompt following a newline proves completion.
                    consumed = len(output); serial.sendall(line.encode()+b'\n'); wait(b'\nsrv> ')
                def drain(seconds):
                    deadline = time.monotonic()+seconds
                    while time.monotonic() < deadline: receive()
                def host_cpu():
                    # Fields 14 and 15 after the parenthesized comm (field 2).
                    fields = Path(f'/proc/{process.pid}/stat').read_text().rsplit(')', 1)[1].split()
                    return (int(fields[11])+int(fields[12]))/os.sysconf('SC_CLK_TCK')
                def measure(name):
                    start = time.monotonic(); cpu = host_cpu(); drain(args.seconds)
                    wall = time.monotonic()-start
                    metrics[name] = {'host_cpu_percent': round(100*(host_cpu()-cpu)/wall, 2), 'wall_seconds': round(wall, 3)}
                def snapshot():
                    start = len(output); command_line('workers')
                    return {f'{slot.decode()}:{name.decode()}': {'ticks': int(ticks), 'switches': int(switches)} for slot,name,ticks,switches in
                        re.findall(rb'scheduler: thread=(\d+) name=(\S+) state=\d+ sampled_ticks=(\d+) switches=(\d+)', output[start:])}
                wait(b'srv> ', 100); command_line('latency reset'); drain(2)
                before = snapshot(); measure('console'); after = snapshot()
                metrics['console']['threads'] = {name: {key: values[key]-before.get(name, {}).get(key, 0) for key in values} for name,values in after.items()}
                serial.sendall(b'run /fat/bin/deskd --apps-trace\n'); wait(b'desktools: starting role=text'); wait(b'deskd: commit'); drain(2)
                measure('desktop')
                # Verify a USB key remains deliverable after the idle interval.
                qmp.key('a'); qmp.key('b', True); drain(.5)
                qmp.call('human-monitor-command', {'command-line': 'sendkey ctrl-l'}); drain(.3)
                for key in ['slash','f','a','t','slash','i','d','l','e','minus','p','r','o','o','f','dot','t','x','t']:
                    qmp.key(key)
                qmp.call('human-monitor-command', {'command-line': 'sendkey ctrl-s'})
                wait(b'picker ready mode=2'); qmp.key('ret'); wait(b'file-result role=text op=2 status=0')
                qmp.call('human-monitor-command', {'command-line': 'sendkey ctrl-alt-esc'})
                wait(b'deskd: exited', 60); wait(b'srv> ')
                command_line('latency'); command_line('workers')
                if b'failure=1' in output or b'deskd: FAIL' in output or b'reason=input-timeout' in output or b'reason=client-timeout' in output:
                    raise RuntimeError('Guest failure; see serial.log')
                if args.max_host_cpu is not None:
                    for name in ('console', 'desktop'):
                        if metrics[name]['host_cpu_percent'] > args.max_host_cpu:
                            raise RuntimeError(f'{name} exceeds host CPU ceiling: {metrics[name]}')
                serial.close(); qmp.stream.close(); qmp.sock.close()
            finally:
                stop_process(process)
                (out/'serial.log').write_bytes(output)
                (out/'metrics.json').write_text(json.dumps(metrics, indent=2)+'\n')
            from exfat_test_image import ExfatFixture
            assert ExfatFixture((tmp/'disk.exfat').read_bytes()).read('idle-proof.txt') == b'aB'
            metrics['status'] = 'PASS'
            (out/'metrics.json').write_text(json.dumps(metrics, indent=2)+'\n')
    print(f'idle: PASS {metrics["console"]} desktop={metrics["desktop"]}; evidence={out}')


if __name__ == '__main__':
    main()
