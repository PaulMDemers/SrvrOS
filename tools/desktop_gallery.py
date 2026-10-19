#!/usr/bin/env python3
"""Capture real desktop screenshots in a disposable VM; never touch host disks.

Build with make all first. Images/logs/manifest go to build/review/desktop-gallery;
review them before copying selected PNGs and the manifest into documentation.
"""
import datetime
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time

from PIL import Image
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso
from exfat_test_image import ExfatFixture
from image_fixtures import image_pattern
from vm_sockets import connect_unix


def main():
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/review/desktop-gallery'
    out.mkdir(parents=True, exist_ok=True)
    # A failed new capture must not leave an old successful manifest behind.
    (out / 'manifest.json').unlink(missing_ok=True)
    source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    worktree_dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=root))
    hashes = {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
              for name in ('build/iso_root/boot/srvros.elf', 'build/iso_root/boot/initramfs.tar', 'build/srvros.exfat')}
    output = b''
    consumed = 0
    shots = []
    with tempfile.TemporaryDirectory(prefix='srvros-gallery-') as directory:
        tmp = Path(directory)
        fixture = ExfatFixture((root / 'build/srvros.exfat').read_bytes())
        fixture.add_directory_files('Gallery', {
            'Welcome.txt': b'SrvrOS\n\nA small operating system, built from scratch.\n\n'
                b'A native desktop for everyday experiments.\n'
                b'A shell and server underneath.\n\n'
                b'Files, Text, Terminal, Preview and Sketch.\n'
                b'Shared controls. Independent applications.\n\n'
                b'Built for learning, testing and improving.\n',
            'Palette.png': image_pattern(),
            'Notes.txt': b'These are synthetic files for the documentation gallery.\n',
        })
        (tmp / 'disk.exfat').write_bytes(fixture.data)
        fixture_hash = hashlib.sha256(fixture.data).hexdigest()
        iso = build_iso(str(root), str(tmp), 1440, 900, os.environ.copy())
        command = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', str(iso),
            '-boot', 'd', '-drive', f'if=none,id=disk,file={tmp / "disk.exfat"},format=raw',
            '-device', 'ich9-ahci,id=ahci', '-device', 'ide-hd,drive=disk,bus=ahci.0',
            '-device', 'qemu-xhci,id=xhci', '-device', 'usb-kbd,bus=xhci.0',
            '-serial', f'unix:{tmp / "serial"},server=on,wait=off',
            '-qmp', f'unix:{tmp / "qmp"},server=on,wait=off', '-display', 'none',
            '-monitor', 'none', '-no-reboot', '-nic', 'none',
            '-rtc', 'base=2026-10-19T12:34:00,clock=vm']
        with (out / 'qemu.log').open('wb') as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            serial = qmp = None
            try:
                serial = connect_unix(tmp / 'serial', 20)
                serial.settimeout(.2)
                qmp = QMP(tmp / 'qmp')

                def wait(marker, timeout=45):
                    nonlocal output, consumed
                    started = time.monotonic()
                    while marker not in output[consumed:]:
                        if marker == b'srv> ' and not consumed:
                            qmp.boot_if_menu_stalled(output, time.monotonic() - started)
                        if time.monotonic() - started > timeout or process.poll() is not None:
                            raise RuntimeError(f'Missing {marker!r}; see {out}')
                        try:
                            chunk = serial.recv(16384)
                            if not chunk:
                                raise RuntimeError('Serial disconnected')
                            output += chunk
                            (out / 'serial.log').write_bytes(output)
                        except socket.timeout:
                            pass
                    consumed = output.index(marker, consumed) + len(marker)

                def shortcut(keys):
                    qmp.call('human-monitor-command', {'command-line': 'sendkey ' + keys})
                    time.sleep(.3)

                def type_text(text):
                    mapping = {' ': 'spc', '/': 'slash', '.': 'dot'}
                    for char in text:
                        qmp.key(mapping.get(char, char.lower()), char.isupper())

                def launch(name, marker):
                    shortcut('meta_l-spc'); wait(b'launcher opened')
                    type_text(name); qmp.key('ret'); wait(marker); wait(b'deskd: commit')

                def shot(name, caption):
                    time.sleep(1)
                    ppm = out / (name + '.ppm')
                    png = out / (name + '.png')
                    qmp.call('screendump', {'filename': str(ppm)})
                    with Image.open(ppm) as frame:
                        if frame.size != (1440, 900):
                            raise RuntimeError(f'Unexpected screen dimensions: {frame.size}')
                        # Lossless format conversion only; preserve every displayed pixel.
                        frame.save(png)
                        with Image.open(png) as saved:
                            if saved.tobytes() != frame.tobytes():
                                raise RuntimeError('Screenshot pixel mismatch')
                    shots.append({'file': png.name, 'caption': caption, 'width': 1440,
                        'height': 900, 'sha256': hashlib.sha256(png.read_bytes()).hexdigest()})

                wait(b'srv> ', 120)
                serial.sendall(b'run /fat/bin/deskd --apps-trace\n')
                wait(b'desktools: starting role=text'); wait(b'deskd: commit')
                shortcut('ctrl-l'); type_text('/fat/Gallery/Welcome.txt')
                shortcut('ctrl-o'); wait(b'picker ready mode=1')
                type_text('Welcome'); qmp.key('ret')
                wait(b'file-result role=text op=1 status=0')
                shortcut('meta_l-alt-left'); wait(b'mode=1 frame=')
                launch('Files', b'desktools: starting role=files')
                shortcut('ctrl-l'); type_text('/fat/Gallery'); qmp.key('ret')
                wait(b'file-result role=files op=3 status=0')
                shortcut('meta_l-alt-right'); wait(b'mode=2 frame=')
                shot('desktop-files-text', 'Text and Files, tiled using shared window controls.')
                launch('Settings', b'desktools: starting role=settings')
                shot('desktop-settings', 'Settings with the shared native widget toolkit.')
                shortcut('ctrl-w'); wait(b'desktools: exited role=settings failure=0')
                launch('Preview', b'deskview: starting')
                shortcut('ctrl-o'); wait(b'deskview: picker ready')
                shortcut('ctrl-l'); type_text('/fat/Gallery'); qmp.key('ret')
                wait(b'deskview: picker ready'); shortcut('tab')
                type_text('Palette.png'); qmp.key('ret')
                wait(b'deskview: loaded path=/fat/Gallery/Palette.png width=1600 height=1000')
                shot('desktop-preview', 'Preview displaying a generated color-grid fixture.')
                shortcut('ctrl-w'); wait(b'deskview: exited')
                shortcut('f10'); shortcut('end'); shortcut('ret')
                wait(b'deskd: exited', 60); wait(b'srv> ')
                if any(marker in output for marker in (b'failure=1', b'deskd: FAIL', b'exception:', b'input-timeout', b'control-timeout')):
                    raise RuntimeError('Guest failure or client timeout; see serial.log')
            except Exception:
                if qmp is not None and process.poll() is None:
                    qmp.call('screendump', {'filename': str(out / 'failure.ppm')})
                raise
            finally:
                stop_process(process)
                if serial is not None:
                    serial.close()
                if qmp is not None:
                    qmp.stream.close(); qmp.sock.close()
                (out / 'serial.log').write_bytes(output)
    manifest = {'schema': 'srvros-desktop-gallery-v1', 'source_commit': source,
        'worktree_dirty': worktree_dirty,
        'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'initial_fixture_disk_sha256': fixture_hash,
        'captured_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'command': 'tools/host-env.sh python3 tools/desktop_gallery.py',
        'environment': 'QEMU q35, 512 MiB, BIOS, 1440x900, 1x, disposable exFAT, no network',
        'guest_clock': 'Fixed 2026-10-19T12:34:00 base; not the capture wall clock',
        'qemu_version': subprocess.check_output(['qemu-system-x86_64', '--version'], text=True).splitlines()[0],
        'artifacts_sha256': hashes, 'images': shots,
        'note': 'Actual guest framebuffer captures, lossless PPM-to-PNG conversion. Synthetic documents and color fixture. VM evidence only; no physical hardware acceptance.'}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'desktop-gallery: PASS {len(shots)} images and clean desktop exit; {out}')


if __name__ == '__main__':
    main()
