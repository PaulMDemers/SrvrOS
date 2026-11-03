#!/usr/bin/env python3
"""Exercise real Text window snap placement using a QMP VirtIO tablet.

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
    out = root / 'build/review/desktop-snap'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    manifest = out / 'source-manifest.json'
    manifest.write_text(json.dumps(source_manifest(root), indent=2, sort_keys=True) + '\n')
    metadata = {'source_manifest_sha256': sha256(manifest),
                'kernel_sha256': sha256(root / 'build/iso_root/boot/srvros.elf'),
                'base_disk_sha256': sha256(root / 'build/srvros.exfat'),
                'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]}
    results = []
    with tempfile.TemporaryDirectory(prefix='srvros-snap-') as directory:
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

                    def traced(pattern, start):
                        drain()
                        matches = re.findall(pattern, output[start:])
                        if not matches:
                            raise AssertionError(f'{mode}: missing trace {pattern!r}')
                        return tuple(map(int, matches[-1]))

                    def preview(which, frame, start):
                        wait(f'deskd: snap-preview mode={which} frame='.encode(), start=start)
                        wait(b'\n')
                        got = traced(rb'deskd: snap-preview mode=(\d+) frame=(-?\d+),(-?\d+),(\d+),(\d+)', start)
                        if got != (which, *frame):
                            raise AssertionError(f'preview {got} != {(which, *frame)}')

                    def title(frame):
                        x, y, w, _ = frame
                        return x + min(120, w // 3), y + 20

                    def no_apply(start):
                        drain()
                        if (b'deskd: snapped identity=' in output[start:] or
                                f'deskd: commit pid={pid} '.encode() in output[start:]):
                            raise AssertionError('placement or client resize occurred unexpectedly')

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
                    original = tuple(initial)
                    phrase = 'snap selection proof'
                    text(phrase); shortcut('ctrl-a'); shortcut('ctrl-c')
                    wait(f'clipboard role=2 action=8 bytes={len(phrase)}'.encode())
                    drain()
                    # The existing placement contract has eight pixel margins and gap.
                    half = (logical_w - 24) // 2
                    targets = {1: (8, 52, half, logical_h-132),
                               2: (logical_w-half-8, 52, half, logical_h-132),
                               4: (8, 52, logical_w-16, logical_h-132)}
                    points = {1: (0, logical_h//2), 2: (logical_w-1, logical_h//2),
                              4: (logical_w//2, 50)}
                    frame = original
                    identity = None
                    for kind in (1, 2, 4):
                        x, y = title(frame); move(x, y); drain(); start = len(output)
                        button(True); move(*points[kind])
                        preview(kind, targets[kind], start)
                        no_apply(start)
                        shot(f'preview-{kind}')
                        button(False)
                        wait(b'deskd: snapped identity=', start=start); wait(b'\n')
                        got = traced(rb'deskd: snapped identity=(\d+) mode=(\d+) frame=(-?\d+),(-?\d+),(\d+),(\d+)', start)
                        if got[1:] != (kind, *targets[kind]):
                            raise AssertionError(f'snap {got} != {(kind, *targets[kind])}')
                        if identity is not None and identity != got[0]:
                            raise AssertionError('window identity changed')
                        identity = got[0]; frame = targets[kind]
                        wait(f'deskd: commit pid={pid} '.encode(), start=start)
                        wait(f'size={frame[2]-2}x{frame[3]-45}'.encode(), start=start)
                        shot(f'settled-{kind}')
                        # Stationary title click preserves placement and does not resize.
                        move(*title(frame)); drain(); click_start = len(output)
                        button(True); button(False); no_apply(click_start)
                        # Dragging a placed window restores its pre-placement size.
                        x, y = title(frame); move(x, y); drain(); restore_start = len(output)
                        button(True); move(logical_w//2, logical_h//2)
                        wait(b'deskd: drag-restored identity=', start=restore_start); wait(b'\n')
                        restored = traced(rb'deskd: drag-restored identity=(\d+) frame=(-?\d+),(-?\d+),(\d+),(\d+)', restore_start)
                        if restored[0] != identity or restored[3:] != original[2:]:
                            raise AssertionError(f'drag restore lost original size: {restored}, {original}')
                        rx, ry, rw, rh = restored[1:]
                        if not (rx <= logical_w//2 < rx+rw and ry <= logical_h//2 < ry+44):
                            raise AssertionError(f'restored title not anchored near pointer: {restored}')
                        button(False); wait(b'deskd: moved identity=', start=restore_start); wait(b'\n')
                        moved = traced(rb'deskd: moved identity=(\d+) frame=(-?\d+),(-?\d+),(\d+),(\d+)', restore_start)
                        if moved[0] != identity or moved[3:] != original[2:]:
                            raise AssertionError(f'unexpected restored move: {moved}')
                        frame = moved[1:]
                        wait(f'size={frame[2]-2}x{frame[3]-45}'.encode(), start=restore_start)
                        shot(f'restored-{kind}')
                    # A direct side-to-side snap retains the original restore frame.
                    restore_frame = frame
                    move(*title(frame)); drain(); start = len(output)
                    button(True); move(*points[1]); preview(1, targets[1], start)
                    no_apply(start); button(False)
                    wait(f'deskd: snapped identity={identity} mode=1 '.encode(), start=start)
                    wait(f'size={targets[1][2]-2}x{targets[1][3]-45}'.encode(), start=start)
                    move(*title(targets[1])); drain(); start = len(output)
                    button(True); move(*points[2]); preview(2, targets[2], start)
                    # Restoring the placed window during drag can commit before
                    # the preview; no additional resize may happen while held.
                    held = len(output); no_apply(held); shot('direct-right-preview')
                    button(False)
                    wait(f'deskd: snapped identity={identity} mode=2 '.encode(), start=start)
                    got = traced(rb'deskd: snapped identity=(\d+) mode=(\d+) frame=(-?\d+),(-?\d+),(\d+),(\d+)', start)
                    if got != (identity, 2, *targets[2]):
                        raise AssertionError(f'direct side placement mismatch: {got}')
                    wait(f'size={targets[2][2]-2}x{targets[2][3]-45}'.encode(), start=start)
                    start = len(output); shortcut('meta_l-alt-down')
                    wait(f'deskd: zoom pid={pid} maximized=0 frame={",".join(map(str, restore_frame))}'.encode(), start=start)
                    wait(f'size={restore_frame[2]-2}x{restore_frame[3]-45}'.encode(), start=start)
                    frame = restore_frame
                    # Leaving the zone clears the preview; release must not place.
                    x, y = title(frame); move(x, y); drain(); start = len(output)
                    button(True); move(*points[1]); preview(1, targets[1], start)
                    clear_start = len(output); move(logical_w//2, logical_h//2)
                    wait(b'deskd: snap-preview mode=0 ', start=clear_start)
                    button(False); no_apply(start)
                    moved = traced(rb'deskd: moved identity=(\d+) frame=(-?\d+),(-?\d+),(\d+),(\d+)', start)
                    frame = moved[1:]; shot('preview-cleared')
                    # Escape cancels capture even if the button remains held.
                    move(*title(frame)); drain(); start = len(output)
                    button(True); move(*points[2]); preview(2, targets[2], start)
                    qmp.key('esc'); wait(b'deskd: snap-preview mode=0 ', start=start)
                    cancelled = len(output); move(logical_w//2, logical_h//2); button(False); no_apply(cancelled)
                    if b'deskd: moved identity=' in output[cancelled:]:
                        raise AssertionError('drag continued after Escape')
                    # The last held motion moves the frame to the right-edge clamp.
                    frame = (logical_w-160, logical_h//2-20, frame[2], frame[3])
                    # Opening a picker while captured revokes capture and preview.
                    move(*title(frame)); drain(); start = len(output)
                    button(True); move(*points[1]); preview(1, targets[1], start)
                    shortcut('ctrl-o'); wait(b'picker ready mode=1', start=start)
                    wait(f'deskd: commit pid={pid} generation=1 '.encode(), start=start)
                    wait(b'deskd: snap-preview mode=0 ', start=start)
                    drain()
                    blocked = len(output); move(*points[2]); button(False); no_apply(blocked)
                    shot('modal-during-preview'); qmp.key('esc'); time.sleep(.5)
                    start = len(output); shortcut('ctrl-c')
                    wait(f'clipboard role=2 action=8 bytes={len(phrase)}'.encode(), start=start)
                    shot('selection-after-snap')
                    shortcut('ctrl-l'); shortcut('ctrl-a'); text('/fat/snap-proof.txt')
                    shortcut('ctrl-s'); wait(b'picker ready mode=2'); qmp.key('ret')
                    wait(b'file-result role=text op=2 status=0')
                    shortcut('f10'); shortcut('end'); shortcut('ret')
                    wait(b'virtio-gpu-present: frames=', 60); wait(b'reset=ok')
                    wait(b'deskd: gfx-present end'); wait(b'deskd: exited', 60); wait(b'srv> ')
                    serial.sendall(b'cat /fat/snap-proof.txt\n'); wait(phrase.encode()); wait(b'srv> ')
                    log = output.decode(errors='replace')
                    if has_fatal_exception(log) or re.search(r'\b(?:FAIL|PANIC)\b', log):
                        raise AssertionError('guest failure reported')
                    qmp.call('quit'); process.wait(timeout=10)
                    results.append({'mode': mode, 'logical': [logical_w, logical_h], 'text_pid': pid,
                                    'text_window': window, 'identity': identity, 'original': original,
                                    'boot_iso_sha256': iso_hash, 'active_pointer': active[0]})
                    print(f'desktop-snap: {mode} PASS', flush=True)
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
        'checks': ['held preview and release placement for left, right and maximize',
                   'stationary placed click and drag restoration', 'preview leave and Escape cancellation',
                   'modal capture cancellation', 'selection and saved content', 'GPU presenter reset'],
        'limitations': 'Screenshots provide visual evidence; host pointer capture and smoothness are not tested.'}, indent=2) + '\n')
    print(f'desktop-snap: PASS; evidence: {out}')


if __name__ == '__main__':
    main()
