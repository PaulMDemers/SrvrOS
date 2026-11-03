#!/usr/bin/env python3
"""Exercise real Text window edge resizing using a QMP VirtIO tablet.

Screenshots provide visual cursor evidence; host-window pointer capture and
smoothness require attended testing. The guest disk is a disposable copy.
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
from desktop_theme_profiles_smoke import qmp_axis, sha256, source_manifest
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso, has_fatal_exception
from virtio_gpu_present_smoke import ppm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    if not 10 <= args.boot_wait <= 240:
        parser.error('--boot-wait must be between 10 and 240')
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/review/desktop-resize'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    manifest = out / 'source-manifest.json'
    manifest.write_text(json.dumps(source_manifest(root), indent=2, sort_keys=True) + '\n')
    metadata = {'source_manifest_sha256': sha256(manifest),
                'kernel_sha256': sha256(root / 'build/iso_root/boot/srvros.elf'),
                'base_disk_sha256': sha256(root / 'build/srvros.exfat'),
                'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]}
    results = []
    with tempfile.TemporaryDirectory(prefix='srvros-resize-') as directory:
        for width, height, scale in ((1280, 800, 1), (1920, 1080, 2)):
            mode = f'{width}x{height}-scale{scale}'
            logical_w, logical_h = (width + scale - 1) // scale, (height + scale - 1) // scale
            work = Path(directory) / mode
            work.mkdir()
            disk = work / 'disk.exfat'
            shutil.copyfile(root / 'build/srvros.exfat', disk)
            iso = build_iso(str(root), str(work), width, height, os.environ.copy(),
                            cmdline='srvros.virtio=on srvros.virtio-gpu=present')
            iso_hash = sha256(Path(iso))
            port = choose_port(30001, 38000)
            command = [args.qemu, '-M', 'q35,i8042=off', '-m', '512M', '-cdrom', str(iso),
                       '-boot', 'd', '-vga', 'none', '-device', 'virtio-vga,disable-legacy=on',
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

                    def wait(marker, timeout=50, boot=False, start=None):
                        nonlocal consumed
                        pos = consumed if start is None else start
                        end = time.monotonic() + timeout
                        while marker not in output[pos:]:
                            if process.poll() is not None or time.monotonic() >= end:
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
                        consumed = max(consumed, output.index(marker, pos) + len(marker))

                    def drain(seconds=.6):
                        end = time.monotonic() + seconds
                        while time.monotonic() < end:
                            try:
                                chunk = serial.recv(16384)
                                if not chunk:
                                    raise RuntimeError('serial disconnected')
                                output.extend(chunk)
                            except socket.timeout:
                                pass

                    def shortcut(keys):
                        qmp.call('human-monitor-command', {'command-line': 'sendkey ' + keys})
                        time.sleep(.3)

                    def text(value):
                        for char in value:
                            qmp.key({' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus'}.get(char, char.lower()), char.isupper())

                    def move(x, y):
                        qmp.call('input-send-event', {'events': [
                            {'type': 'abs', 'data': {'axis': 'x', 'value': qmp_axis(x, logical_w)}},
                            {'type': 'abs', 'data': {'axis': 'y', 'value': qmp_axis(y, logical_h)}}]})
                        time.sleep(.25)

                    def button(down):
                        qmp.call('input-send-event', {'events': [{'type': 'btn', 'data': {'button': 'left', 'down': down}}]})
                        time.sleep(.25)

                    def shot(name):
                        time.sleep(.5)
                        path = out / f'{mode}-{name}.ppm'
                        qmp.call('screendump', {'filename': str(path)})
                        w, h, _ = ppm(path)
                        if (w, h) != (width, height):
                            raise AssertionError(f'wrong framebuffer {w}x{h}')

                    def latest_resize(start, edges):
                        wait(b'deskd: resized identity=', start=start)
                        wait(b'\n')
                        matches = re.findall(rb'deskd: resized identity=(\d+) edges=(\d+) frame=(-?\d+),(-?\d+),(\d+),(\d+)', output[start:consumed])
                        if not matches:
                            raise AssertionError('malformed resize trace')
                        identity, got_edges, *frame = map(int, matches[-1])
                        if got_edges != edges:
                            raise AssertionError(f'resize edges {got_edges} != {edges}')
                        return identity, tuple(frame)

                    def point(frame, edges):
                        x, y, w, h = frame
                        return (x-2 if edges & 1 else x+w+1 if edges & 2 else x+w//2,
                                y-2 if edges & 4 else y+h+1 if edges & 8 else y+h//2)

                    wait(b'srv> ', args.boot_wait, boot=True)
                    if b'virtio: input=ready' not in output or b'virtio-gpu-present: ready' not in output:
                        raise AssertionError('VirtIO input or presenter not ready')
                    mice = qmp.call('query-mice')
                    (out / f'mice-{mode}.json').write_text(json.dumps(mice, indent=2) + '\n')
                    active = [m for m in mice if m.get('current')]
                    if len(active) != 1 or not active[0].get('absolute'):
                        raise AssertionError(f'active pointer is not absolute: {mice!r}')
                    serial.sendall(f'run /fat/bin/deskd --apps-trace --scale={scale}\n'.encode())
                    wait(f'deskd: display physical={width}x{height} logical={logical_w}x{logical_h} scale={scale}'.encode())
                    wait(b'desktools: starting role=text'); wait(b'deskd: commit'); wait(b'\n')
                    geometry = re.findall(rb'deskd: geometry pid=(\d+) window=(\d+) frame=(\d+),(\d+),(\d+),(\d+)', output)
                    if not geometry:
                        raise AssertionError('Text geometry missing')
                    pid, window, *initial = map(int, geometry[0])
                    frame = tuple(initial)
                    phrase = 'resize selection proof'
                    text(phrase); shortcut('ctrl-a'); shortcut('ctrl-c')
                    wait(f'clipboard role=2 action=8 bytes={len(phrase)}'.encode())
                    identities = set()
                    # Leave enough room for the outward drag and its return
                    # halo above the dock, which has priority over windows.
                    if frame[1] + frame[3] + 14 >= logical_h - 74:
                        x, y = point(frame, 8); move(x, y); start = len(output)
                        button(True); move(x, y-32); button(False)
                        identity, normalized = latest_resize(start, 8)
                        expected = (frame[0], frame[1], frame[2], frame[3]-32)
                        if normalized != expected:
                            raise AssertionError(f'could not clear dock: {normalized} != {expected}')
                        wait(f'size={normalized[2]-2}x{normalized[3]-45}'.encode(), start=start)
                        drain(); identities.add(identity); frame = normalized
                        initial = list(frame)
                    if not (frame[0] >= 14 and frame[1] >= 58 and
                            frame[0]+frame[2]+14 < logical_w and frame[1]+frame[3]+14 < logical_h):
                        raise AssertionError(f'Text frame lacks resize halo and travel: {frame}')
                    # A click without movement must retain the exact frame and
                    # must not request a new client buffer.
                    x, y = point(frame, 2); move(x, y); drain(); start = len(output)
                    button(True); button(False)
                    _, stationary = latest_resize(start, 2)
                    drain()
                    if stationary != frame or b'deskd: commit pid=' in output[start:]:
                        raise AssertionError(f'stationary resize changed frame or committed: {stationary}')
                    # Two small opposite drags per edge prove exact initial anchoring
                    # and restore the starting frame before the next direction.
                    for name, edges in (('left', 1), ('right', 2), ('top', 4), ('bottom', 8),
                                        ('top-left', 5), ('top-right', 6), ('bottom-left', 9), ('bottom-right', 10)):
                        for outward in (True, False):
                            x, y = point(frame, edges)
                            cursor_start = len(output)
                            move(x, y)
                            if outward:
                                wait(f'deskd: cursor shape={edges}'.encode(), start=cursor_start)
                                shot('cursor-' + name)
                            dx = (-12 if edges & 1 else 12 if edges & 2 else 0) * (1 if outward else -1)
                            dy = (-12 if edges & 4 else 12 if edges & 8 else 0) * (1 if outward else -1)
                            start = len(output)
                            button(True); move(x+dx, y+dy); button(False)
                            identity, actual = latest_resize(start, edges)
                            identities.add(identity)
                            fx, fy, fw, fh = frame
                            expected = (fx+dx if edges & 1 else fx, fy+dy if edges & 4 else fy,
                                        fw-dx if edges & 1 else fw+dx if edges & 2 else fw,
                                        fh-dy if edges & 4 else fh+dy if edges & 8 else fh)
                            if actual != expected:
                                raise AssertionError(f'{name}: frame {actual} != expected {expected}')
                            frame = actual
                            wait(f'deskd: commit pid={pid} '.encode(), start=start)
                            wait(f'size={actual[2]-2}x{actual[3]-45}'.encode(), start=start)
                            drain()
                        if frame != tuple(initial):
                            raise AssertionError(f'{name}: failed to restore initial frame')
                    if len(identities) != 1:
                        raise AssertionError(f'resize changed window identity: {identities}')
                    cursor_start = len(output)
                    move(logical_w-1, logical_h-1)
                    wait(b'deskd: cursor shape=0', start=cursor_start)
                    shot('cursor-arrow-after-leave')
                    # Shrink past both limits; the opposite corner stays fixed.
                    x, y = point(frame, 10); start = len(output)
                    button_x, button_y = frame[0] + 2, frame[1] + 2
                    move(x, y); button(True); move(button_x, button_y); button(False)
                    _, minimum = latest_resize(start, 10)
                    if minimum != (frame[0], frame[1], 282, 285):
                        raise AssertionError(f'minimum frame or anchor incorrect: {minimum}')
                    wait(f'deskd: commit pid={pid} '.encode(), start=start)
                    wait(b'size=280x240', start=start)
                    shot('minimum-size')
                    x, y = point(minimum, 10); start = len(output)
                    move(x, y); button(True)
                    move(x + frame[2]-minimum[2], y + frame[3]-minimum[3]); button(False)
                    _, restored = latest_resize(start, 10)
                    if restored != frame:
                        raise AssertionError(f'failed to restore after minimum: {restored} != {frame}')
                    wait(f'deskd: commit pid={pid} '.encode(), start=start)
                    wait(f'size={frame[2]-2}x{frame[3]-45}'.encode(), start=start)
                    shortcut('ctrl-c'); wait(f'clipboard role=2 action=8 bytes={len(phrase)}'.encode())
                    shot('selection-after-resize')
                    # Escape ends capture without rolling back the last size.
                    # Moving while the button is still held must not resize it.
                    x, y = point(frame, 2); move(x, y); drain(); start = len(output)
                    button(True); move(x+12, y)
                    wait(f'size={frame[2]+10}x{frame[3]-45}'.encode(), start=start)
                    qmp.key('esc'); wait(b'deskd: cursor shape=0', start=start)
                    drain(); start = len(output)
                    move(x+42, y); button(False); drain()
                    if b'deskd: commit pid=' in output[start:] or b'deskd: resized identity=' in output[start:]:
                        raise AssertionError('resize continued after Escape')
                    cancelled_frame = (frame[0], frame[1], frame[2]+12, frame[3])
                    cx, cy = point(cancelled_frame, 2); move(cx, cy); start = len(output)
                    button(True); button(False)
                    _, actual = latest_resize(start, 2)
                    if actual != cancelled_frame:
                        raise AssertionError(f'Escape frame changed: {actual} != {cancelled_frame}')
                    cx, cy = point(cancelled_frame, 2); move(cx, cy); start = len(output)
                    button(True); move(cx-12, cy); button(False)
                    _, restored = latest_resize(start, 2)
                    if restored != frame:
                        raise AssertionError(f'Escape restore failed: {restored} != {frame}')
                    wait(f'size={frame[2]-2}x{frame[3]-45}'.encode(), start=start); drain()
                    # A modal file picker blocks resizing the parent.
                    modal_start = len(output)
                    shortcut('ctrl-o'); wait(b'picker ready mode=1')
                    wait(f'deskd: commit pid={pid} generation=1 '.encode(), start=modal_start)
                    drain(); start = len(output)
                    # The picker overlaps the parent's left side, but its top
                    # starts lower; probe the exposed parent top halo instead.
                    x, y = point(frame, 4); move(x, y); button(True); move(x, y+20); button(False)
                    drain()
                    resized = re.findall(rb'deskd: resized identity=(\d+)', output[start:])
                    if str(next(iter(identities))).encode() in resized:
                        raise AssertionError('modal parent resized')
                    shot('modal-parent'); qmp.key('esc'); time.sleep(.5)
                    # A successful parent clipboard action proves the modal has
                    # closed and keyboard focus has returned to the Text app.
                    start = len(output); shortcut('ctrl-c')
                    wait(f'clipboard role=2 action=8 bytes={len(phrase)}'.encode(), start=start)
                    drain()
                    # Start capture first, then open a modal while the button
                    # remains down. The newly modal parent must lose capture.
                    x, y = point(frame, 4); move(x, y); drain()
                    button(True); modal_start = len(output)
                    shortcut('ctrl-o'); wait(b'picker ready mode=1', start=modal_start)
                    wait(f'deskd: commit pid={pid} generation=1 '.encode(), start=modal_start)
                    drain(); start = len(output)
                    move(x, y+20); button(False); drain()
                    resized = re.findall(rb'deskd: resized identity=(\d+)', output[start:])
                    if str(next(iter(identities))).encode() in resized:
                        raise AssertionError('parent capture survived modal opening')
                    shot('modal-during-capture'); qmp.key('esc'); time.sleep(.5)
                    start = len(output); shortcut('ctrl-c')
                    wait(f'clipboard role=2 action=8 bytes={len(phrase)}'.encode(), start=start)
                    drain()
                    # A fresh stationary probe exposes the actual parent frame,
                    # including any change that happened before release.
                    x, y = point(frame, 4); move(x, y); start = len(output)
                    button(True); button(False)
                    _, after_modal_capture = latest_resize(start, 4)
                    if after_modal_capture != frame:
                        raise AssertionError(f'parent geometry changed during modal: {after_modal_capture} != {frame}')
                    drain()
                    # Release at the display edge and return: a new movement must
                    # not continue the released resize capture.
                    x, y = point(frame, 2); move(x, y); start = len(output)
                    button(True); move(logical_w-1, y); button(False)
                    _, frame = latest_resize(start, 2)
                    drain(); start = len(output)
                    move(logical_w//2, logical_h//2); drain()
                    if (b'deskd: resized identity=' in output[start:] or
                            f'deskd: commit pid={pid} '.encode() in output[start:]):
                        raise AssertionError('resize remained captured after edge release')
                    shot('edge-release')
                    # Avoid an unsaved-changes prompt while exiting.
                    shortcut('ctrl-l'); shortcut('ctrl-a'); text('/fat/resize-proof.txt')
                    shortcut('ctrl-s'); wait(b'picker ready mode=2'); qmp.key('ret')
                    wait(b'file-result role=text op=2 status=0')
                    shortcut('f10'); shortcut('end'); shortcut('ret')
                    wait(b'virtio-gpu-present: frames=', 60); wait(b'reset=ok')
                    wait(b'deskd: gfx-present end'); wait(b'deskd: exited', 60); wait(b'srv> ')
                    serial.sendall(b'cat /fat/resize-proof.txt\n'); wait(phrase.encode()); wait(b'srv> ')
                    log = output.decode(errors='replace')
                    if has_fatal_exception(log) or re.search(r'\b(?:FAIL|PANIC)\b', log):
                        raise AssertionError('guest failure reported')
                    qmp.call('quit'); process.wait(timeout=10)
                    results.append({'mode': mode, 'logical': [logical_w, logical_h], 'text_pid': pid,
                                    'text_window': window, 'identity': next(iter(identities)),
                                    'boot_iso_sha256': iso_hash, 'active_pointer': active[0]})
                    print(f'desktop-resize: {mode} PASS', flush=True)
                except BaseException:
                    if qmp and process and process.poll() is None:
                        try:
                            qmp.call('screendump', {'filename': str(out / f'{mode}-failure.ppm')})
                        except Exception:
                            pass
                    raise
                finally:
                    (out / f'serial-{mode}.log').write_bytes(output)
                    if serial: serial.close()
                    if qmp: qmp.stream.close(); qmp.sock.close()
                    if process: stop_process(process)
    (out / 'result.json').write_text(json.dumps({'status': 'PASS', 'metadata': metadata, 'modes': results,
        'checks': ['eight resize directions and anchored opposite edges', 'minimum frame bounds',
                   'directional cursor traces and arrow after leave', 'selection and saved content',
                   'stationary press and Escape cancellation', 'modal parent capture and capture cancellation', 'edge release and reentry', 'GPU presenter reset'],
        'limitations': 'Cursor screenshots provide visual evidence; host pointer capture and smoothness are not tested.'}, indent=2) + '\n')
    print(f'desktop-resize: PASS; evidence: {out}')


if __name__ == '__main__':
    main()
