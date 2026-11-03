#!/usr/bin/env python3
"""Exercise Settings display trials, persistence, and a live Text document with QMP.

Uses a disposable disk and a matched, already-built ISO tree. Does not build the
OS. Requires the VirtIO GPU mode list and the Settings display controls.
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
    out = root / 'build/review/desktop-display'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    manifest = out / 'source-manifest.json'
    manifest.write_text(json.dumps(source_manifest(root), indent=2, sort_keys=True) + '\n')
    metadata = {'source_manifest_sha256': sha256(manifest),
                'kernel_sha256': sha256(root / 'build/iso_root/boot/srvros.elf'),
                'base_disk_sha256': sha256(root / 'build/srvros.exfat'),
                'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]}
    output = bytearray()
    consumed = 0
    process = serial = qmp = None
    logical = [1440, 900]
    with tempfile.TemporaryDirectory(prefix='srvros-display-') as directory:
        work = Path(directory)
        disk = work / 'disk.exfat'
        shutil.copyfile(root / 'build/srvros.exfat', disk)
        iso = build_iso(str(root), str(work), 1440, 900, os.environ.copy(),
                        cmdline='srvros.virtio=on srvros.virtio-gpu=present')
        metadata['boot_iso_sha256'] = sha256(Path(iso))
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
        (out / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
        started = time.monotonic()
        with (out / 'qemu.log').open('wb') as errors:
            try:
                process = subprocess.Popen(command, stdout=errors, stderr=errors)
                serial = connect_serial(port, 20); serial.settimeout(.2)
                qmp = QMP(work / 'qmp')

                def wait(marker, timeout=50, boot=False, start=None):
                    nonlocal consumed
                    pos = consumed if start is None else start
                    end = time.monotonic() + timeout
                    while marker not in output[pos:]:
                        if process.poll() is not None or time.monotonic() >= end:
                            raise RuntimeError(f'missing {marker!r}; see serial.log')
                        if boot:
                            qmp.boot_if_menu_stalled(output, time.monotonic() - started)
                        try:
                            chunk = serial.recv(16384)
                            if not chunk:
                                raise RuntimeError('serial disconnected')
                            output.extend(chunk); (out / "serial.log").write_bytes(output)
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
                            output.extend(chunk); (out / "serial.log").write_bytes(output)
                        except socket.timeout:
                            pass

                def shortcut(keys):
                    qmp.call('human-monitor-command', {'command-line': 'sendkey ' + keys})
                    time.sleep(.25)

                def text(value):
                    for char in value:
                        qmp.key({' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus'}.get(char, char.lower()), char.isupper())

                def move(x, y):
                    qmp.call('input-send-event', {'events': [
                        {'type': 'abs', 'data': {'axis': 'x', 'value': qmp_axis(x, logical[0])}},
                        {'type': 'abs', 'data': {'axis': 'y', 'value': qmp_axis(y, logical[1])}}]})
                    time.sleep(.2)

                def button(down):
                    qmp.call('input-send-event', {'events': [{'type': 'btn', 'data': {'button': 'left', 'down': down}}]})
                    time.sleep(.2)

                def click(x, y):
                    move(x, y); button(True); button(False)

                def shot(name, size):
                    time.sleep(.6)
                    path = out / (name + '.ppm')
                    qmp.call('screendump', {'filename': str(path)})
                    w, h, _ = ppm(path)
                    if (w, h) != size:
                        raise AssertionError(f'{name}: screenshot {w}x{h}, expected {size}')

                def geometry(pid):
                    drain()
                    matches = re.findall(rb'deskd: geometry pid=' + pid + rb' window=\d+ frame=(\d+),(\d+),(\d+),(\d+)', output)
                    if not matches:
                        raise AssertionError('missing window geometry')
                    return tuple(map(int, matches[-1]))

                def scroll_bottom():
                    # Settings remains focused. Page Down saturates at the end.
                    for _ in range(12):
                        shortcut('pgdn')

                def display_button(which):
                    # Maximized Settings occupies the standard work area. The
                    # scrollbar is at its end; Settings content height is 1912.
                    content_h = logical[1] - 177
                    offset = 1912 - (content_h - 108)
                    local_y = 64 + 1610 + (72 if which in ('mode', 'scale') else 112) - offset + 16
                    x = {'mode': 24+55, 'scale': 172+36, 'apply': 24+40,
                         'keep': 24+40, 'revert': 120+40}[which]
                    click(8 + 1 + x, 52 + 44 + local_y)

                def applied(w, h, scale, restore):
                    marker = f'deskd: display-applied physical={w}x{h} logical={(w+scale-1)//scale}x{(h+scale-1)//scale} scale={scale} restore={restore}'.encode()
                    wait(marker, 45)
                    logical[:] = [(w+scale-1)//scale, (h+scale-1)//scale]

                def desktop_exit():
                    shortcut('f10'); shortcut('end'); shortcut('ret')
                    wait(b'deskd: gfx-present end', 60); wait(b'deskd: exited', 60); wait(b'srv> ')

                wait(b'srv> ', args.boot_wait, boot=True)
                if b'virtio: input=ready' not in output or b'virtio-gpu-present: ready' not in output:
                    raise AssertionError('VirtIO input or presenter not ready')
                mice = qmp.call('query-mice')
                active = [m for m in mice if m.get('current')]
                if len(active) != 1 or not active[0].get('absolute'):
                    raise AssertionError(f'expected absolute tablet: {mice!r}')
                metadata['active_pointer'] = active[0]
                serial.sendall(b'run /fat/bin/deskd --apps-trace\n')
                wait(b'deskd: display physical=1440x900 logical=1440x900 scale=1')
                wait(b'desktools: starting role=text'); wait(b'deskd: commit')
                opened = re.findall(rb'deskd: opened pid=(\d+) window=\d+ id=(\d+) parent=0', output)
                if len(opened) != 1:
                    raise AssertionError('expected one initial Text window')
                text_pid = opened[0][0]
                phrase = 'display selection proof'
                text(phrase); shortcut('ctrl-a'); shortcut('ctrl-c')
                wait(f'clipboard role=2 action=8 bytes={len(phrase)}'.encode())
                # Launcher position at 1440x900, following the existing settings smoke.
                click(logical[0]//2 + 150, logical[1] - 50); wait(b'desktools: starting role=settings'); wait(b'deskd: geometry pid='); wait(b'\n')
                settings_pid = re.findall(rb'deskd: geometry pid=(\d+) window=\d+ frame=', output)[-1]
                sx, sy, sw, sh = geometry(settings_pid)
                click(sx + sw - 56, sy + 20)
                wait(b'deskd: zoom pid=' + settings_pid + b' maximized=1 ')
                scroll_bottom()
                # Index zero is inherited1440x900, then deduplicated presets
                # 1024x768,1280x800,1920x1080.
                for _ in range(3): display_button('mode')
                display_button('scale'); display_button('apply')
                applied(1920, 1080, 2, 0); shot('trial-1920x1080-2x', (1920, 1080))
                scroll_bottom(); display_button('keep')
                wait(b'deskd: display-kept scale=2 resolution=1920x1080')
                wait(b'status=1 ', 60)
                # Keep Settings open while returning to the original Text window.
                click(logical[0]//2 - 150, logical[1]-50); shortcut('ctrl-c')
                wait(f'clipboard role=2 action=8 bytes={len(phrase)}'.encode())
                # A snap triggers a real client resize with its original PID.
                click(logical[0]//2 - 150, logical[1]-50); shortcut('meta_l-alt-left')
                wait(b'deskd: arranged pid=' + text_pid + b' mode=1 '); wait(b'deskd: commit pid=' + text_pid)
                shortcut('ctrl-l'); shortcut('ctrl-a'); text('/fat/display-proof.txt')
                shortcut('ctrl-s'); wait(b'picker ready mode=2'); qmp.key('ret')
                wait(b'file-result role=text op=2 status=0')
                # Reopen Settings via its taskbar, and try the next candidate.
                click(logical[0]//2 + 150, logical[1]-50); scroll_bottom()
                display_button('mode'); display_button('apply')
                applied(2560, 1440, 2, 0); shot('timeout-trial', (2560, 1440))
                trial_start = len(output)
                qmp.call('input-send-event', {'events': [{'type': 'key', 'data': {
                    'down': True, 'key': {'type': 'qcode', 'data': 'alt'}}}]})
                qmp.key('tab'); wait(b'deskd: switcher selected=')
                shot('switcher-during-trial', (2560, 1440))
                applied(1920, 1080, 2, 1); wait(b'deskd: display-reverted', 30)
                qmp.call('input-send-event', {'events': [{'type': 'key', 'data': {
                    'down': False, 'key': {'type': 'qcode', 'data': 'alt'}}}]})
                drain()
                if b'deskd: focused pid=' + settings_pid not in output[trial_start:] or b'deskd: switcher activated' in output[trial_start:]:
                    raise AssertionError('rollback did not cancel switcher and restore Settings focus')
                shot('timeout-reverted', (1920, 1080))
                scroll_bottom(); display_button('mode'); display_button('apply')
                applied(2560, 1440, 2, 0)
                scroll_bottom(); display_button('revert')
                applied(1920, 1080, 2, 1); wait(b'deskd: display-reverted')
                shot('explicit-revert', (1920, 1080))
                scroll_bottom(); display_button('mode'); display_button('apply')
                applied(2560, 1440, 2, 0)
                # Closing the owner must revert even before its deadline.
                click(logical[0]-28, 72)
                applied(1920, 1080, 2, 1); wait(b'deskd: display-reverted')
                shot('owner-close-reverted', (1920, 1080))
                desktop_exit()
                document_start = len(output)
                serial.sendall(b'cat /fat/display-proof.txt\n')
                wait(phrase.encode(), start=document_start)
                wait(b'srv> ')
                readback = bytes(output[document_start:consumed])
                # The shell may echo its command; the document itself has no
                # trailing newline and the next bytes are the shell prompt.
                match = re.search(re.escape(phrase.encode()) + rb'(?:\r?\n)?srv> ', readback)
                if not match:
                    raise AssertionError(f'document bytes differ: {readback!r}')
                serial.sendall(b'cat /fat/home/desktop.conf\n')
                wait(b'display_scale=2\r\ndisplay_width=1920\r\ndisplay_height=1080\r\n'); wait(b'srv> ')
                logical[:] = [1440, 900]
                serial.sendall(b'run /fat/bin/deskd --apps-trace\n')
                applied(1920, 1080, 2, 0)
                shot('restored-after-restart', (1920, 1080))
                desktop_exit()
                log = output.decode(errors='replace')
                if has_fatal_exception(log) or re.search(r'\b(?:FAIL|PANIC)\b', log):
                    raise AssertionError('guest failure reported')
                qmp.call('quit'); process.wait(timeout=10)
            except BaseException:
                if qmp and process and process.poll() is None:
                    try:
                        qmp.call('screendump', {'filename': str(out / 'failure.ppm')})
                    except Exception:
                        pass
                raise
            finally:
                (out / 'serial.log').write_bytes(output)
                if serial: serial.close()
                if qmp: qmp.stream.close(); qmp.sock.close()
                if process: stop_process(process)
    (out / 'result.json').write_text(json.dumps({'status': 'PASS', 'metadata': metadata,
        'checks': ['Settings apply and keep', 'Text selection, snap, resize and saved document',
                   'timed rollback with held switcher; explicit and owner-close rollback', 'saved preference and fresh desktop restore'],
        'limitations': 'Requires current preset ordering and Settings layout. Screenshots prove dimensions, not native glyph detail.'}, indent=2) + '\n')
    print(f'desktop-display: PASS; evidence: {out}')


if __name__ == '__main__':
    main()
