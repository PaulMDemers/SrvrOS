#!/usr/bin/env python3
"""Exercise the opt-in modern VirtIO desktop baseline on a disposable disk.

Requires a freshly built ISO staging tree and build/srvros.exfat. Evidence and
failure screenshots are written to build/review/virtio-baseline; no entropy
bytes are printed or saved by the guest probe.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso, has_fatal_exception
from virtio_gpu_present_smoke import ppm
from virtio_gpu_probe_smoke import provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--boot-wait', type=int, default=120)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    args = parser.parse_args()
    if not 10 <= args.boot_wait <= 240:
        parser.error('--boot-wait must be between 10 and 240')
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/review/virtio-baseline'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    metadata = provenance(root)
    marker = 'virtio-persist-' + str(time.time_ns())
    with tempfile.TemporaryDirectory(prefix='srvros-virtio-baseline-') as directory:
        tmp = Path(directory)
        disk = tmp / 'disk.exfat'
        shutil.copyfile(root / 'build/srvros.exfat', disk)
        iso = build_iso(str(root), str(tmp), 1280, 800, os.environ.copy(),
                        cmdline='srvros.virtio=on srvros.virtio-gpu=present')
        port = choose_port(30001, 38000)
        command = [args.qemu, '-M', 'q35,i8042=off', '-m', '512M', '-cdrom', str(iso), '-boot', 'd',
                   '-vga', 'none', '-device', 'virtio-vga,disable-legacy=on',
                   '-device', 'virtio-keyboard-pci,disable-legacy=on',
                   '-device', 'virtio-mouse-pci,disable-legacy=on',
                   '-object', 'rng-random,id=rng0,filename=/dev/urandom',
                   '-device', 'virtio-rng-pci,rng=rng0,disable-legacy=on',
                   '-drive', f'if=none,id=disk,file={disk},format=raw',
                   '-device', 'virtio-blk-pci,drive=disk,disable-legacy=on',
                   '-netdev', 'user,id=user0',
                   '-object', f'filter-dump,id=capture0,netdev=user0,file={out / "network.pcap"}',
                   '-device', 'virtio-net-pci,netdev=user0,id=net0,disable-legacy=on',
                   '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off',
                   '-qmp', f'unix:{tmp / "qmp"},server=on,wait=off',
                   '-display', 'none', '-monitor', 'none', '-no-reboot']
        (out / 'command.json').write_text(json.dumps(command, indent=2) + '\n')

        def session(number):
            output = bytearray()
            consumed = 0
            qmp = serial = process = None
            started = time.monotonic()
            with (out / f'qemu-{number}.log').open('wb') as errors:
                try:
                    process = subprocess.Popen(command, stdout=errors, stderr=errors)
                    serial = connect_serial(port, 20)
                    serial.settimeout(.2)
                    qmp = QMP(tmp / 'qmp')

                    def wait(value, timeout=45, boot=False):
                        nonlocal consumed
                        deadline = time.monotonic() + timeout
                        while value not in output[consumed:]:
                            if process.poll() is not None or time.monotonic() > deadline:
                                raise RuntimeError(f'missing {value!r}; see serial-{number}.log')
                            if boot:
                                qmp.boot_if_menu_stalled(output, time.monotonic() - started)
                            try:
                                chunk = serial.recv(16384)
                                if not chunk:
                                    raise RuntimeError('serial disconnected')
                                output.extend(chunk)
                            except socket.timeout:
                                pass
                        consumed = output.index(value, consumed) + len(value)

                    def monitor(command_text, expected=None, timeout=45):
                        serial.sendall(command_text.encode() + b'\n')
                        start = consumed
                        # Editing redraws "srv> " on every character. Only a
                        # prompt after the submitted line's newline is terminal.
                        wait(b'\n', timeout)
                        wait(b'srv> ', timeout)
                        result = bytes(output[start:consumed])
                        if expected is not None and expected not in result:
                            raise AssertionError(f'{command_text}: missing {expected!r}: {result!r}')
                        return result

                    def type_text(value):
                        mapping = {' ': ('spc', False), '/': ('slash', False),
                                   '.': ('dot', False), '-': ('minus', False),
                                   '_': ('minus', True)}
                        for char in value:
                            key, shift = mapping.get(char, (char.lower(), char.isupper()))
                            qmp.key(key, shift)

                    def shortcut(keys):
                        qmp.call('human-monitor-command', {'command-line': 'sendkey ' + keys})
                        time.sleep(.3)

                    def shot(name, colors=16):
                        time.sleep(.5)
                        path = out / f'{name}.ppm'
                        qmp.call('screendump', {'filename': str(path)})
                        return ppm(path, colors)

                    wait(b'srv> ', args.boot_wait, boot=True)
                    boot_log = bytes(output[:consumed])
                    stats = monitor('virtio')
                    monitor('pci')
                    for value in (b'virtio: input=ready rng=ready',
                                  b'virtio-gpu-present: ready',
                                  b'exfat: mounted /fat from virtio0 ',
                                  b'virtio-net: ready'):
                        if value not in boot_log:
                            raise AssertionError(f'boot missing {value!r}')
                    if b'exfat: trying initramfs fallback' in boot_log:
                        raise AssertionError('disk mount used initramfs fallback')
                    inp = re.search(rb'virtio-input: devices=(\d+) keyboards=(\d+) mice=(\d+) failed=(\d+) quarantined=(\d+)', stats)
                    if not inp or tuple(map(int, inp.groups())) != (2, 1, 1, 0, 0):
                        raise AssertionError(f'input inventory invalid: {stats!r}')
                    block = re.search(rb'virtio-block: virtio0 sectors=\d+ writable=yes flush=yes online=yes quarantine=no transport=ok admission=15 selector=1 firmware_reset=1 reset_ok=1 commands=\d+ reads=(\d+) writes=(\d+) flushes=(\d+) failures=(\d+)', stats)
                    if not block or int(block.group(1)) == 0 or int(block.group(4)) != 0:
                        raise AssertionError(f'block device not online: {stats!r}')
                    print(f'virtio-baseline: boot {number} devices and mounted disk PASS',flush=True)
                    if number == 2:
                        monitor('cat /fat/virtio-proof.txt', marker.encode())
                        if has_fatal_exception(output.decode(errors='replace')):
                            raise AssertionError('guest exception on second boot')
                        qmp.call('quit')
                        process.wait(timeout=10)
                        return bytes(output)

                    # QMP input, not serial injection, must execute this monitor command.
                    type_text('echo virtio keyboard ok'); qmp.key('ret')
                    wait(b'\nvirtio keyboard ok\r\n'); wait(b'srv> ')
                    monitor('run /fat/bin/ifconfig', b'driver=virtio-net role=active-net0')
                    qmp.call('set_link', {'name':'net0','up':False})
                    monitor('run /fat/bin/ifconfig', b'driver=virtio-net role=active-net0 link=down')
                    qmp.call('set_link', {'name':'net0','up':True})
                    monitor('run /fat/bin/ifconfig', b'driver=virtio-net role=active-net0 link=up')
                    serial.sendall(b'run /fat/bin/sh\n')
                    wait(b'srvsh ready.'); wait(b' $ ')
                    serial.sendall(b'dhcp\n')
                    dhcp_start=consumed
                    wait(b'dhcp:'); wait(b'\n'); wait(b' $ ',60)
                    dhcp_ok=b'dhcp: 10.0.2.' in output[dhcp_start:consumed]
                    serial.sendall(b'exit\n'); wait(b'srv> ')
                    if not dhcp_ok:
                        monitor('virtio')
                        raise AssertionError('DHCP failed; device counters and packet capture retained')
                    ping = monitor('run /fat/bin/ping 10.0.2.2', b'reply from 10.0.2.2:', 80)
                    if not re.search(rb'ping: sent=4 received=[1-4] lost=', ping):
                        raise AssertionError(f'ping did not complete: {ping!r}')
                    monitor('run /fat/bin/nodeprobe', b'nodeprobe: time/random ok', 120)
                    stats = monitor('virtio')
                    rng = re.search(rb'virtio-rng: ready=1 received=(\d+) consumed=(\d+) buffered=\d+ quarantined=0', stats)
                    if not rng or min(map(int, rng.groups())) <= 0:
                        raise AssertionError(f'RNG did not receive and supply entropy: {stats!r}')
                    net = re.search(rb'virtio-net: ready rx=(\d+) tx=(\d+)', stats)
                    if not net or min(map(int, net.groups())) <= 0:
                        raise AssertionError(f'network had no completed traffic: {stats!r}')
                    print('virtio-baseline: keyboard, link, DHCP, ping and entropy PASS',flush=True)

                    monitor(f'write /fat/virtio-proof.txt {marker}', b'write: /fat/virtio-proof.txt')
                    stats = monitor('virtio')
                    block = re.search(rb'virtio-block: virtio0 .*? writes=(\d+) flushes=(\d+) failures=(\d+)', stats)
                    if not block or int(block.group(1)) == 0 or int(block.group(3)) != 0:
                        raise AssertionError(f'block write was not submitted: {stats!r}')
                    serial.sendall(b'run /fat/bin/deskd --apps-trace\n')
                    wait(b'desktools: starting role=text'); wait(b'deskd: commit')
                    # Verify Text received the real keyboard events through clipboard length.
                    shortcut('ctrl-a'); type_text('Virtio typing')
                    shortcut('ctrl-a'); shortcut('ctrl-c')
                    wait(b'clipboard role=2 action=8 bytes=13')
                    shortcut('ctrl-l'); shortcut('ctrl-a'); type_text('/fat/virtio-text.txt')
                    shortcut('ctrl-shift-s'); wait(b'picker ready mode=2')
                    qmp.key('ret'); wait(b'file-result role=text op=2 status=0')
                    w, h, before = shot('desktop-before-drag')
                    # Initial cursor (24,56), Text title bar at (180,80).
                    qmp.mouse(156,24); qmp.mouse(0,0,1); qmp.mouse(120,70,1); qmp.mouse(0,0,0)
                    wait(b'deskd: moved identity=')
                    w2, h2, after = shot('desktop-after-drag')
                    if (w,h) != (w2,h2) or sum(a != b for a,b in zip(before,after)) < 3000:
                        raise AssertionError('window drag did not update scanout')
                    shortcut('f1'); wait(b'deskhelp: page=text'); wait(b'deskd: commit')
                    shortcut('ctrl-l'); wait(b'deskhelp: topics opened')
                    type_text('keyboard'); qmp.key('ret'); wait(b'deskhelp: page=keyboard')
                    # Place the pointer over the Help document and send a real wheel event.
                    qmp.mouse(100,126)
                    for down in (True, False):
                        qmp.call('input-send-event', {'events': [{'type': 'btn', 'data':
                            {'down': down, 'button': 'wheel-down'}}]})
                        time.sleep(.1)
                    scroll_start = consumed
                    wait(b'deskhelp: scroll='); wait(b'\n')
                    scroll = re.search(rb'deskhelp: scroll=(\d+)', output[scroll_start:consumed])
                    if not scroll or int(scroll.group(1)) <= 0:
                        raise AssertionError('wheel did not scroll Help down')
                    shot('help-after-wheel')
                    shortcut('f10'); shortcut('end'); shortcut('ret')
                    wait(b'virtio-gpu-present: frames='); wait(b'reset=ok')
                    wait(b'deskd: exited', 60); wait(b'srv> ')
                    shot('console-after-exit', colors=2)
                    # reboot performs process_file_sync_all and block_flush_all;
                    # -no-reboot exits QEMU only after that guest path succeeds.
                    serial.sendall(b'reboot\n')
                    deadline = time.monotonic() + 60
                    while process.poll() is None and time.monotonic() < deadline:
                        try:
                            chunk = serial.recv(16384)
                            if chunk: output.extend(chunk)
                        except socket.timeout: pass
                    if process.poll() is None or process.returncode != 0:
                        raise RuntimeError('guest reboot/flush did not complete')
                    text = output.decode(errors='replace')
                    if 'reboot: storage flush failed' in text or has_fatal_exception(text) or re.search(r'\b(?:FAIL|PANIC)\b', text):
                        raise AssertionError('guest reported failure')
                    return bytes(output)
                except BaseException:
                    if qmp and process and process.poll() is None:
                        try: qmp.call('screendump', {'filename': str(out / f'failure-{number}.ppm')})
                        except Exception: pass
                    raise
                finally:
                    (out / f'serial-{number}.log').write_bytes(output)
                    if serial: serial.close()
                    if qmp: qmp.stream.close(); qmp.sock.close()
                    if process: stop_process(process)

        session(1)
        (tmp / 'qmp').unlink(missing_ok=True)
        session(2)
        (out / 'result.json').write_text(json.dumps({
            'status': 'PASS', 'provenance': metadata,
            'checks': ['modern input and QMP keyboard', 'Text typing', 'mouse drag and wheel',
                       'GPU presentation and clean reset', 'RNG receive and consumption',
                       'VirtIO net DHCP and ping', 'VirtIO disk mount and reboot persistence']
        }, indent=2) + '\n')
    print(f'virtio-baseline: PASS; evidence: {out}')


if __name__ == '__main__':
    main()
