#!/usr/bin/env python3
"""Test VirtIO tablet QMP routing and logical desktop input in two VM modes.

This is headless QMP evidence, not a test of host-window cursor release. The
Cocoa window-entry/exit behavior requires a separate attended host test.
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

# QMP InputMoveEvent absolute coordinates are inclusive 0..0x7fff.
# https://www.qemu.org/docs/master/interop/qemu-qmp-ref.html#object-inputmoveevent
ABS_MAX = 32767


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    if not 10 <= args.boot_wait <= 240:
        parser.error('--boot-wait must be between 10 and 240')
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/review/virtio-tablet'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    metadata = provenance(root)
    results = []
    with tempfile.TemporaryDirectory(prefix='srvros-virtio-tablet-') as directory:
        tmp = Path(directory)
        for width, height, scale in ((1280, 800, 1), (1920, 1080, 2)):
            logical_w, logical_h = (width + scale - 1)//scale, (height + scale - 1)//scale
            mode = f'{width}x{height}-scale{scale}'
            work = tmp / mode
            work.mkdir()
            disk = work / 'disk.exfat'
            shutil.copyfile(root / 'build/srvros.exfat', disk)
            iso = build_iso(str(root), str(work), width, height, os.environ.copy(),
                            cmdline='srvros.virtio=on srvros.virtio-gpu=present')
            port = choose_port(30001, 38000)
            command = [args.qemu, '-M', 'q35,i8042=off', '-m', '512M',
                       '-cdrom', str(iso), '-boot', 'd', '-vga', 'none',
                       '-device', 'virtio-vga,disable-legacy=on',
                       '-device', 'virtio-keyboard-pci,disable-legacy=on',
                       '-device', 'virtio-tablet-pci,disable-legacy=on',
                       '-drive', f'if=none,id=disk,file={disk},format=raw',
                       '-device', 'ich9-ahci,id=ahci', '-device', 'ide-hd,drive=disk,bus=ahci.0',
                       '-nic', 'none', '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off',
                       '-qmp', f'unix:{work / "qmp"},server=on,wait=off',
                       '-display', 'none', '-monitor', 'none', '-no-reboot']
            (out / f'command-{mode}.json').write_text(json.dumps(command, indent=2) + '\n')
            output = bytearray()
            consumed = 0
            qmp = serial = process = None
            started = time.monotonic()
            with (out / f'qemu-{mode}.log').open('wb') as errors:
                try:
                    process = subprocess.Popen(command, stdout=errors, stderr=errors)
                    serial = connect_serial(port, 20)
                    serial.settimeout(.2)
                    qmp = QMP(work / 'qmp')

                    def wait(marker, timeout=50, boot=False):
                        nonlocal consumed
                        end = time.monotonic() + timeout
                        while marker not in output[consumed:]:
                            if process.poll() is not None or time.monotonic() > end:
                                raise RuntimeError(f'{mode}: missing {marker!r}; see serial log')
                            if boot:
                                qmp.boot_if_menu_stalled(output, time.monotonic() - started)
                            try:
                                chunk = serial.recv(16384)
                                if not chunk:
                                    raise RuntimeError('serial disconnected')
                                output.extend(chunk)
                            except socket.timeout:
                                pass
                        consumed = output.index(marker, consumed) + len(marker)

                    def drain(seconds=.6):
                        end = time.monotonic() + seconds
                        while time.monotonic() < end:
                            try:
                                chunk = serial.recv(16384)
                                if not chunk: raise RuntimeError('serial disconnected')
                                output.extend(chunk)
                            except socket.timeout:
                                pass

                    def command_monitor(line):
                        serial.sendall(line.encode() + b'\n')
                        # The line editor redraws the prompt during typing.
                        wait(b'\n'); wait(b'srv> ')

                    def shortcut(keys):
                        qmp.call('human-monitor-command', {'command-line': 'sendkey ' + keys})
                        time.sleep(.3)

                    def text(value):
                        for char in value:
                            qmp.key({' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus'}.get(char, char.lower()), char.isupper())

                    def absolute(x, y):
                        if not (0 <= x <= ABS_MAX and 0 <= y <= ABS_MAX):
                            raise ValueError('invalid QMP absolute coordinate')
                        qmp.call('input-send-event', {'events': [
                            {'type': 'abs', 'data': {'axis': 'x', 'value': x}},
                            {'type': 'abs', 'data': {'axis': 'y', 'value': y}}]})

                    def logical(x, y):
                        # Invert both floor mappings: QMP/tablet 0..32767 ->
                        # input ABI 0..65535 -> logical pixel. Inverting only
                        # the endpoints can land one pixel short at scale 2.
                        def axis(pixel, extent):
                            normalized = (pixel * 65535 + extent-2)//(extent-1)
                            return (normalized * ABS_MAX + 65534)//65535
                        absolute(axis(x,logical_w), axis(y,logical_h))

                    def button(name, down):
                        qmp.call('input-send-event', {'events': [
                            {'type': 'btn', 'data': {'button': name, 'down': down}}]})
                        time.sleep(.2)

                    def pointer(x, y, buttons=0):
                        wait(f'deskd: pointer x={x} y={y} buttons={buttons}'.encode()); wait(b'\n')

                    def shot(name):
                        time.sleep(.5)
                        path = out / f'{mode}-{name}.ppm'
                        qmp.call('screendump', {'filename': str(path)})
                        w, h, pixels = ppm(path)
                        if (w, h) != (width, height):
                            raise AssertionError(f'wrong framebuffer {w}x{h}')
                        return pixels

                    def finish_desktop():
                        wait(b'virtio-gpu-present: frames=', 60); wait(b'reset=ok')
                        wait(b'deskd: gfx-present end'); wait(b'deskd: exited', 60); wait(b'srv> ')

                    wait(b'srv> ', args.boot_wait, boot=True)
                    boot_log = bytes(output[:consumed])
                    if b'virtio: input=ready' not in boot_log or b'virtio-gpu-present: ready' not in boot_log:
                        raise AssertionError('VirtIO input or GPU not ready')
                    mice = qmp.call('query-mice')
                    (out / f'mice-{mode}.json').write_text(json.dumps(mice, indent=2) + '\n')
                    active = [m for m in mice if m.get('current')]
                    if len(active) != 1 or not active[0].get('absolute'):
                        raise AssertionError(f'active QMP pointer is not absolute: {mice!r}')
                    serial.sendall(f'run /fat/bin/deskd --trace --scale={scale}\n'.encode())
                    wait(f'deskd: display physical={width}x{height} logical={logical_w}x{logical_h} scale={scale}'.encode())
                    wait(b'deskd: ready;')
                    # All four endpoints plus center and a far-edge return prove
                    # absolute jumps and logical scaling without a relative device.
                    for ax, ay, px, py in ((0,0,0,0), (ABS_MAX,0,logical_w-1,0),
                                           (ABS_MAX,ABS_MAX,logical_w-1,logical_h-1),
                                           (0,ABS_MAX,0,logical_h-1),
                                           (16384,16384,(logical_w-1)//2,(logical_h-1)//2),
                                           (0,0,0,0),
                                           (ABS_MAX,ABS_MAX,logical_w-1,logical_h-1)):
                        absolute(ax,ay); pointer(px,py)
                    # Hold a title-bar drag through an edge, release, then jump
                    # back in. The trace verifies button release and no stuck drag.
                    logical(100,80); pointer(100,80)
                    button('left',True); pointer(100,80,1)
                    absolute(ABS_MAX,ABS_MAX); pointer(logical_w-1,logical_h-1,1)
                    button('left',False); pointer(logical_w-1,logical_h-1,0)
                    drain(); before_return = len(output)
                    logical(logical_w//2,logical_h//2); pointer(logical_w//2,logical_h//2,0)
                    drain()
                    if b'deskd: moved identity=' in output[before_return:]:
                        raise AssertionError('window kept dragging after edge release')
                    shot('edge-release')
                    qmp.key('esc'); finish_desktop()

                    # Start real Text/Help apps and verify a click, drag and wheel.
                    serial.sendall(f'run /fat/bin/deskd --apps-trace --scale={scale}\n'.encode())
                    wait(b'desktools: starting role=text'); wait(b'deskd: commit')
                    logical(180,230); button('left',True); button('left',False)
                    shortcut('ctrl-a'); text('tablet proof'); shortcut('ctrl-a'); shortcut('ctrl-c')
                    wait(b'clipboard role=2 action=8 bytes=12')
                    # Save before quitting so the Text app does not prompt on exit.
                    shortcut('ctrl-l'); shortcut('ctrl-a'); text('/fat/tablet-proof.txt')
                    shortcut('ctrl-s'); wait(b'picker ready mode=2'); qmp.key('ret')
                    wait(b'file-result role=text op=2 status=0')
                    before = shot('before-drag')
                    logical(180,80); button('left',True); logical(260,140); button('left',False)
                    wait(b'deskd: moved identity=')
                    after = shot('after-drag')
                    if sum(a != b for a,b in zip(before,after)) < 3000:
                        raise AssertionError('absolute drag did not alter scanout')
                    shortcut('f1'); wait(b'deskhelp: page=text'); wait(b'deskd: commit')
                    shortcut('ctrl-l'); wait(b'deskhelp: topics opened'); text('keyboard')
                    qmp.key('ret'); wait(b'deskhelp: page=keyboard')
                    logical(logical_w//2, logical_h//2)
                    button('wheel-down',True); button('wheel-down',False)
                    start = consumed
                    wait(b'deskhelp: scroll='); wait(b'\n')
                    match = re.search(rb'deskhelp: scroll=(\d+)', output[start:consumed])
                    if not match or int(match.group(1)) == 0:
                        raise AssertionError('wheel did not scroll Help')
                    shot('help-wheel')
                    shortcut('f10'); shortcut('end'); shortcut('ret'); finish_desktop()
                    log = output.decode(errors='replace')
                    if has_fatal_exception(log) or re.search(r'\b(?:FAIL|PANIC)\b', log):
                        raise AssertionError('guest failure reported')
                    qmp.call('quit'); process.wait(timeout=10)
                    results.append({'physical': [width,height], 'logical': [logical_w,logical_h], 'scale': scale,
                                    'active_pointer': active[0]})
                    print(f'virtio-tablet: {mode} QMP absolute and desktop PASS', flush=True)
                except BaseException:
                    if qmp and process and process.poll() is None:
                        try: qmp.call('screendump', {'filename': str(out / f'{mode}-failure.ppm')})
                        except Exception: pass
                    raise
                finally:
                    (out / f'serial-{mode}.log').write_bytes(output)
                    if serial: serial.close()
                    if qmp: qmp.stream.close(); qmp.sock.close()
                    if process: stop_process(process)
    (out / 'result.json').write_text(json.dumps({
        'status': 'PASS', 'modes': results, 'provenance': metadata,
        'checks': ['QMP active absolute pointer', 'logical corners and center', 'edge release and reentry',
                   'Text click and window drag', 'Help wheel', 'GPU presenter reset'],
        'host_cursor_release': 'not tested: requires attended Cocoa window entry/exit'
    }, indent=2) + '\n')
    print(f'virtio-tablet: PASS QMP guest routing; host cursor release requires attended verification; evidence: {out}')


if __name__ == '__main__':
    main()
