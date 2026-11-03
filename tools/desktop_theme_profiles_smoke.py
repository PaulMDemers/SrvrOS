#!/usr/bin/env python3
"""Exercise live and persisted theme profiles with QMP VirtIO input.

Uses one disposable disk and two desktop sessions. Screenshots are visual
inspection evidence; QMP timing does not establish host-window smoothness.
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

from desktop_apps_smoke import selection_color
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso, has_fatal_exception
from virtio_gpu_present_smoke import ppm
from serial_records import desktop_progress_end

ABS_MAX = 32767
WIDTH, HEIGHT = 1280, 800


def sha256(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def source_manifest(root):
    def git(*args):
        return subprocess.check_output(['git', *args], cwd=root)
    paths = git('ls-files', '-co', '--exclude-standard', '-z').split(b'\0')
    files = {}
    for raw in paths:
        if not raw:
            continue
        name = os.fsdecode(raw)
        path = root / name
        if path.is_file() and not path.is_symlink():
            files[name] = sha256(path)
    return {'commit': git('rev-parse', 'HEAD').decode().strip(),
            'status': git('status', '--porcelain=v1').decode(errors='replace'),
            'files_sha256': files}


def qmp_axis(pixel, extent):
    """Invert QMP 0..32767 -> ABI 0..65535 -> logical pixels, rounding up."""
    if not 0 <= pixel < extent or extent < 2:
        raise ValueError('logical coordinate outside screen')
    normalized = (pixel * 65535 + extent - 2) // (extent - 1)
    return (normalized * ABS_MAX + 65534) // 65535


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    if not 10 <= args.boot_wait <= 240:
        parser.error('--boot-wait must be between 10 and 240')
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/review/desktop-theme-profiles'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    manifest = source_manifest(root)
    (out / 'source-manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    metadata = {'submodules': subprocess.check_output(['git', 'submodule', 'status', '--recursive'], cwd=root, text=True),
                'source_manifest_sha256': sha256(out / 'source-manifest.json'),
                'kernel_sha256': sha256(root / 'build/iso_root/boot/srvros.elf'),
                'base_disk_sha256': sha256(root / 'build/srvros.exfat'),
                'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]}
    output = bytearray()
    consumed = 0
    markers = []
    with tempfile.TemporaryDirectory(prefix='srvros-theme-profiles-') as directory:
        tmp = Path(directory)
        disk = tmp / 'disk.exfat'
        shutil.copyfile(root / 'build/srvros.exfat', disk)
        iso = build_iso(str(root), str(tmp), WIDTH, HEIGHT, os.environ.copy(),
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
                   '-qmp', f'unix:{tmp / "qmp"},server=on,wait=off',
                   '-display', 'none', '-monitor', 'none', '-no-reboot']
        (out / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
        qmp = serial = process = None
        started = time.monotonic()
        with (out / 'qemu.log').open('wb') as errors:
            try:
                process = subprocess.Popen(command, stdout=errors, stderr=errors)
                serial = connect_serial(port, 20)
                serial.settimeout(.2)
                qmp = QMP(tmp / 'qmp')

                def wait(marker, timeout=50, boot=False, start=None):
                    nonlocal consumed
                    end = time.monotonic() + timeout
                    search_from = consumed if start is None else start
                    while (matched_end := desktop_progress_end(output, marker, search_from)) is None:
                        if process.poll() is not None or time.monotonic() >= end:
                            raise RuntimeError(f'missing {marker!r}; see serial.log')
                        if boot:
                            qmp.boot_if_menu_stalled(output, time.monotonic() - started)
                        try:
                            chunk = serial.recv(16384)
                            if not chunk:
                                raise RuntimeError('serial disconnected')
                            output.extend(chunk)
                        except socket.timeout:
                            pass
                    consumed = max(consumed, matched_end)
                    markers.append(marker.decode(errors='replace'))

                def shortcut(keys):
                    qmp.call('human-monitor-command', {'command-line': 'sendkey ' + keys})
                    time.sleep(.3)

                def text(value):
                    for char in value:
                        qmp.key({' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus'}.get(char, char.lower()), char.isupper())

                def move(x, y):
                    qmp.call('input-send-event', {'events': [
                        {'type': 'abs', 'data': {'axis': 'x', 'value': qmp_axis(x, WIDTH)}},
                        {'type': 'abs', 'data': {'axis': 'y', 'value': qmp_axis(y, HEIGHT)}}]})

                def button(name, down):
                    qmp.call('input-send-event', {'events': [{'type': 'btn', 'data': {'button': name, 'down': down}}]})
                    time.sleep(.2)

                def click(x, y):
                    move(x, y); button('left', True); button('left', False)

                def shot(name):
                    time.sleep(.5)
                    path = out / (name + '.ppm')
                    qmp.call('screendump', {'filename': str(path)})
                    w, h, pixels = ppm(path)
                    if (w, h) != (WIDTH, HEIGHT):
                        raise AssertionError(f'wrong screenshot dimensions: {w}x{h}')
                    return pixels

                def finish():
                    shortcut('f10'); shortcut('end'); shortcut('ret')
                    wait(b'virtio-gpu-present: frames=', 60); wait(b'reset=ok')
                    wait(b'deskd: gfx-present end'); wait(b'deskd: exited', 60); wait(b'srv> ')

                def frame(pid):
                    pattern = rb'deskd: geometry pid=' + pid + rb' window=\d+ frame=(\d+),(\d+),(\d+),(\d+)'
                    matches = re.findall(pattern, output)
                    if not matches:
                        raise AssertionError('missing traced window geometry')
                    return tuple(map(int, matches[-1]))

                wait(b'srv> ', args.boot_wait, boot=True)
                if b'virtio: input=ready' not in output or b'virtio-gpu-present: ready' not in output:
                    raise AssertionError('VirtIO input or presenter not ready')
                mice = qmp.call('query-mice')
                (out / 'mice.json').write_text(json.dumps(mice, indent=2) + '\n')
                active = [m for m in mice if m.get('current')]
                if len(active) != 1 or not active[0].get('absolute'):
                    raise AssertionError(f'active pointer is not absolute: {mice!r}')
                serial.sendall(b'run /fat/bin/deskd --apps-trace --scale=1\n')
                wait(b'deskd: display physical=1280x800 logical=1280x800 scale=1')
                wait(b'desktools: starting role=text'); wait(b'deskd: commit')
                original = re.findall(rb'deskd: opened pid=(\d+) window=\d+ id=(\d+) parent=0', output)
                if len(original) != 1:
                    raise AssertionError('expected one original Text window')
                text_pid, text_window = original[0]
                phrase = 'profile selection proof'
                text(phrase)
                shortcut('ctrl-l'); shortcut('ctrl-a'); text('/fat/home/profile-proof.txt')
                shortcut('ctrl-s'); wait(b'picker ready mode=2'); qmp.key('ret')
                wait(b'file-result role=text op=2 status=0')
                click(90, 225); shortcut('ctrl-a'); shortcut('ctrl-c')
                wait(b'clipboard role=2 action=8 bytes=23')
                shot('silver-existing')
                silver = selection_color(out / 'silver-existing.ppm', 49, 64+44+102)
                # Keep Files alive while Settings broadcasts each profile.
                files_start = len(output)
                click(390, 750); wait(b'desktools: starting role=files', start=files_start)
                wait(b'deskd: geometry pid=', start=files_start); wait(b'\n')
                files_geometry = re.findall(rb'deskd: geometry pid=(\d+) window=\d+ frame=(\d+),(\d+),(\d+),(\d+)', output[files_start:])[-1]
                files_pid = files_geometry[0]
                fx, fy, fw, fh = map(int, files_geometry[1:])
                wait(b'files-location path=/fat/home ', start=files_start)
                wait(b'deskd: commit pid=' + files_pid, start=files_start)
                if fw - 2 < 708 or fx + fw >= WIDTH or fy + fh >= HEIGHT:
                    raise AssertionError(f'Files normal frame unsuitable for resize: {(fx, fy, fw, fh)}')
                text('profile-proof')

                def files_selection():
                    shortcut('f2')
                    wait(b'files-prompt operation=5 name=profile-proof.txt')
                    qmp.key('esc'); wait(b'deskd: retired pid=' + files_pid)

                def files_resize(current_width, target_width):
                    # Drag the actual bottom-right resize grip. Zoom and restore
                    # logs expose the compositor's resulting frame for assertion.
                    # The compositor derives frame size from absolute pointer
                    # position, not from the distance dragged from the grip.
                    move(fx + current_width - 10, fy + fh - 10)
                    button('left', True); move(fx + target_width - 1, fy + fh - 1); button('left', False)
                    time.sleep(.5)
                    click(fx + target_width - 56, fy + 20)
                    wait(b'deskd: zoom pid=' + files_pid + b' maximized=1 ')
                    click(WIDTH - 64, 72)
                    wait(f'deskd: zoom pid={files_pid.decode()} maximized=0 frame={fx},{fy},{target_width},{fh}'.encode())

                def files_profile(name):
                    click(390, 750); files_selection(); shot(name + '-files-normal')
                    files_resize(fw, 600); files_selection(); shot(name + '-files-compact')
                    files_resize(600, fw); files_selection()

                files_profile('silver')
                # A second Files window shares the owner but has its own history.
                sibling_start = len(output)
                shortcut('ctrl-shift-n')
                wait(b'files-runtime: admitted window=', start=sibling_start); wait(b'\n')
                sibling = re.findall(rb'files-runtime: admitted window=(\d+)', output[sibling_start:])[-1]
                wait(b'files-location path=/fat/home ', start=sibling_start)
                wait(b'deskd: commit pid=' + files_pid, start=sibling_start)
                shortcut('ctrl-l'); shortcut('ctrl-a'); text('/fat'); qmp.key('ret')
                wait(b'files-location path=/fat history=2/2')
                shortcut('alt-left'); wait(b'files-location path=/fat/home history=1/2')
                shortcut('ctrl-w'); wait(b'files-runtime: closed window=' + sibling + b' ')
                files_selection()
                click(790, 750); wait(b'desktools: starting role=settings')
                wait(b'deskd: geometry pid='); wait(b'\n')
                geometry = re.findall(rb'deskd: geometry pid=(\d+) window=\d+ frame=(\d+),(\d+),(\d+),(\d+)', output)
                if not geometry:
                    raise AssertionError('Settings geometry missing')
                settings_pid, sx, sy, sw, sh = geometry[-1]
                sx, sy, sw, sh = map(int, (sx, sy, sw, sh))
                client_w = sw - 2
                column = (client_w - 64) // 3
                for style, name in ((1, 'aero'), (2, 'aqua')):
                    click(sx + 1 + 24 + style * column + 15, sy + 44 + 172 + 16)
                    wait(f'theme_accent=0 glass_level=1 theme_style={style}'.encode())
                    wait(b'status=1 ')
                    shot(name + '-settings')
                    files_profile(name)
                    click(490, 750); shortcut('ctrl-c')
                    wait(b'clipboard role=2 action=8 bytes=23')
                    shot(name + '-existing')
                    current = selection_color(out / (name + '-existing.ppm'), 49, 64+44+102)
                    if current == silver:
                        raise AssertionError(f'{name}: original Text selection did not repaint')
                    shortcut('f10'); shortcut('right'); shot(name + '-popup'); qmp.key('esc')
                    shortcut('ctrl-o'); wait(b'picker ready mode=1'); shot(name + '-dialog'); qmp.key('esc')
                    shortcut('ctrl-shift-n'); wait(b'text-runtime: admitted window='); wait(b'\n')
                    new_window = re.findall(rb'text-runtime: admitted window=(\d+)', output)[-1]
                    if new_window == text_window:
                        raise AssertionError('new Text window reused original ID')
                    text(phrase); shortcut('ctrl-a'); shot(name + '-new')
                    nx, ny, _, _ = frame(text_pid)
                    inherited = selection_color(out / (name + '-new.ppm'), nx+17, ny+44+102)
                    if inherited != current:
                        raise AssertionError(f'{name}: new Text selection {inherited} differs from existing {current}')
                    qmp.key('backspace'); shortcut('ctrl-w'); wait(b'text-runtime: closed window=' + new_window + b' ')
                    click(790, 750)
                click(490, 750)
                move(180, 80); button('left', True); move(260, 140); button('left', False)
                wait(b'deskd: moved identity='); shot('aqua-drag')
                shortcut('f1'); wait(b'deskhelp: page=text'); wait(b'deskd: commit')
                shortcut('ctrl-l'); wait(b'deskhelp: topics opened'); text('keyboard')
                qmp.key('ret'); wait(b'deskhelp: page=keyboard')
                move(640, 400); button('wheel-down', True); button('wheel-down', False)
                start = consumed; wait(b'deskhelp: scroll='); wait(b'\n')
                match = re.search(rb'deskhelp: scroll=(\d+)', output[start:consumed])
                if not match or int(match.group(1)) == 0:
                    raise AssertionError('Help wheel did not scroll')
                shot('aqua-help-wheel'); finish()
                serial.sendall(b'cat /fat/home/desktop.conf\n'); wait(b'theme_style=2'); wait(b'srv> ')
                serial.sendall(b'cat /fat/home/profile-proof.txt\n'); wait(phrase.encode()); wait(b'srv> ')
                restart_start = len(output)
                serial.sendall(b'run /fat/bin/deskd --apps-trace --scale=1\n')
                wait(b'status=1 text=16', start=restart_start)
                wait(b'theme_accent=0 glass_level=1 theme_style=2', start=restart_start)
                wait(b'desktools: starting role=text', start=restart_start)
                wait(b'deskd: commit', start=restart_start)
                shot('aqua-restarted'); finish()
                log = output.decode(errors='replace')
                if has_fatal_exception(log) or re.search(r'\b(?:FAIL|PANIC)\b', log):
                    raise AssertionError('guest failure reported')
                qmp.call('quit'); process.wait(timeout=10)
                metadata['final_disk_sha256'] = sha256(disk)
            except BaseException:
                if qmp and process and process.poll() is None:
                    try:
                        qmp.call('screendump', {'filename': str(out / 'failure.ppm')})
                    except Exception:
                        pass
                raise
            finally:
                (out / 'serial.log').write_bytes(output)
                (out / 'markers.json').write_text(json.dumps(markers, indent=2) + '\n')
                if serial: serial.close()
                if qmp: qmp.stream.close(); qmp.sock.close()
                if process: stop_process(process)
    (out / 'result.json').write_text(json.dumps({
        'status': 'PASS', 'metadata': metadata, 'active_pointer': active[0],
        'checks': ['live Aero and Aqua', 'existing and new Text selection', 'content and selection retained',
                   'Files selection across profiles and normal/compact resize', 'Files sibling history and close',
                   'popup and dialog captures', 'drag and Help wheel', 'saved Aqua and desktop restart'],
        'limitations': 'QMP guest evidence does not test host-window smoothness or cursor release.'
    }, indent=2) + '\n')
    print(f'desktop-theme-profiles: PASS; QMP guest evidence: {out}')


if __name__ == '__main__':
    main()
