#!/usr/bin/env python3
"""Capture a presentation screenshot set from a hidden srvros QEMU boot."""

import argparse
import json
import os
from pathlib import Path
import random
import shutil
import socket
import subprocess
import sys
import tempfile
import time


LAUNCH_SEQUENCE = [
    ("04-notes.png", 80, 59, b"mapped surface window NOTES"),
    ("05-files.png", 80, 93, b"mapped surface window FILES"),
    ("06-text-edit.png", 80, 127, b"mapped surface window TEXT EDIT"),
    ("07-paint.png", 80, 161, b"mapped surface window PAINT"),
    ("08-gui2-demo.png", 80, 195, b"mapped surface window GUI2 DEMO"),
    ("09-surface-demo.png", 80, 229, b"mapped surface window SURFACE DEMO"),
    ("10-calculator.png", 80, 263, b"mapped surface window CALC"),
]


def default_root():
    return Path(__file__).resolve().parents[1]


def choose_port(start, end):
    for _ in range(300):
        port = random.randint(start, end)
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            try:
                probe.bind(("127.0.0.1", port))
                return port
            except OSError:
                pass
    raise RuntimeError("no free local port found")


def connect_tcp(port, timeout, label):
    deadline = time.time() + timeout
    last_error = None
    while time.time() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1)
        except OSError as exc:
            last_error = exc
            time.sleep(0.15)
    raise RuntimeError(f"{label} connection failed: {last_error}")


def read_for(sock, seconds):
    end = time.time() + seconds
    chunks = []
    while time.time() < end:
        try:
            data = sock.recv(4096)
            if data:
                chunks.append(data)
        except socket.timeout:
            pass
    return b"".join(chunks)


def read_until_any(sock, markers, timeout):
    deadline = time.time() + timeout
    data = b""
    while time.time() < deadline:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                continue
            data += chunk
            for marker in markers:
                if marker in data:
                    return marker, data
        except socket.timeout:
            pass
    raise RuntimeError(f"timeout waiting for {markers!r}; tail={data[-500:]!r}")


def qmp_read_response(sock, timeout=10):
    deadline = time.time() + timeout
    buffer = b""
    while time.time() < deadline:
        chunk = sock.recv(65536)
        if not chunk:
            continue
        buffer += chunk
        while b"\r\n" in buffer:
            line, buffer = buffer.split(b"\r\n", 1)
            if not line:
                continue
            try:
                message = json.loads(line.decode("utf-8", "replace"))
            except json.JSONDecodeError:
                continue
            if "return" in message or "error" in message:
                return message
    raise RuntimeError("qmp response timeout")


def qmp_command(sock, command):
    sock.sendall(json.dumps(command).encode("ascii") + b"\r\n")
    response = qmp_read_response(sock)
    if "error" in response:
        raise RuntimeError(f"qmp command failed: {command!r}: {response!r}")
    return response


def hmp(sock, command):
    return qmp_command(sock, {
        "execute": "human-monitor-command",
        "arguments": {
            "command-line": command,
        },
    })


def send_qmp_key(sock, key):
    qmp_command(sock, {
        "execute": "input-send-event",
        "arguments": {
            "events": [
                {
                    "type": "key",
                    "data": {
                        "down": True,
                        "key": {
                            "type": "qcode",
                            "data": key,
                        },
                    },
                },
                {
                    "type": "key",
                    "data": {
                        "down": False,
                        "key": {
                            "type": "qcode",
                            "data": key,
                        },
                    },
                },
            ],
        },
    })


def send_qmp_text(sock, text, key_delay):
    qcodes = {
        "\n": "ret",
        " ": "spc",
        "/": "slash",
        "-": "minus",
        ".": "dot",
    }
    for digit in "0123456789":
        qcodes[digit] = digit
    for char in text:
        lower = char.lower()
        if lower in qcodes:
            send_qmp_key(sock, qcodes[lower])
        elif "a" <= lower <= "z":
            send_qmp_key(sock, lower)
        else:
            continue
        if key_delay > 0:
            time.sleep(key_delay)


def capture_screen(qmp, path):
    raw_path = path.with_suffix(".ppm")
    if path.exists():
        path.unlink()
    if raw_path.exists():
        raw_path.unlink()
    hmp(qmp, "screendump " + str(raw_path).replace("\\", "/"))
    deadline = time.time() + 8
    while time.time() < deadline:
        if raw_path.exists() and raw_path.stat().st_size > 0:
            try:
                from PIL import Image
                Image.open(raw_path).save(path)
                raw_path.unlink()
            except Exception:
                if path.suffix.lower() == ".ppm":
                    raw_path.replace(path)
                else:
                    raw_path.replace(path)
            return
        time.sleep(0.1)
    raise RuntimeError(f"screenshot was not written: {path}")


class Mouse:
    def __init__(self, qmp):
        self.qmp = qmp
        for _ in range(50):
            hmp(qmp, "mouse_move -80 -80")
            time.sleep(0.005)
        self.x = 0
        self.y = 0
        time.sleep(0.1)

    def move_to(self, x, y):
        dx = x - self.x
        dy = y - self.y
        while dx != 0 or dy != 0:
            step_x = max(-80, min(80, dx))
            step_y = max(-80, min(80, dy))
            hmp(self.qmp, f"mouse_move {step_x} {step_y}")
            self.x += step_x
            self.y += step_y
            dx = x - self.x
            dy = y - self.y
            time.sleep(0.035)
        time.sleep(0.08)

    def click(self, x, y):
        self.move_to(x, y)
        hmp(self.qmp, "mouse_button 1")
        time.sleep(0.08)
        hmp(self.qmp, "mouse_button 0")
        time.sleep(0.30)


def make_contact_sheet(paths, output):
    try:
        from PIL import Image, ImageDraw
    except Exception as exc:
        print(f"presentation-screens: contact sheet skipped: {exc}", file=sys.stderr)
        return None

    tiles = []
    for path in paths:
        image = Image.open(path).convert("RGB")
        image.thumbnail((384, 240))
        tile = Image.new("RGB", (420, 292), (20, 26, 32))
        tile.paste(image, ((420 - image.width) // 2, 28))
        ImageDraw.Draw(tile).text((12, 8), path.name, fill=(235, 244, 250))
        tiles.append(tile)

    columns = 3
    rows = (len(tiles) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * 420, rows * 292), (8, 12, 16))
    for index, tile in enumerate(tiles):
        sheet.paste(tile, ((index % columns) * 420, (index // columns) * 292))
    sheet.save(output)
    return output


def assert_clean_desktop(path):
    try:
        from PIL import Image
    except Exception as exc:
        print(f"presentation-screens: desktop assertions skipped: {exc}", file=sys.stderr)
        return

    image = Image.open(path).convert("RGB")
    width, height = image.size
    if width < 400 or height < 300:
        raise RuntimeError(f"desktop screenshot unexpectedly small: {width}x{height}")

    # The desktop used to show three diagnostic panels in the workspace. Keep a
    # focused crop over that former area so the release screenshots stay clean.
    x0 = min(width - 1, max(0, width // 8 + width // 80))
    y0 = min(height - 1, max(0, height // 24 + height // 80))
    x1 = min(width, x0 + max(120, width // 2))
    y1 = min(height, y0 + max(80, height // 4))
    old_panel_colors = {
        (0x15, 0x23, 0x2d),
        (0x14, 0x26, 0x20),
        (0x24, 0x1c, 0x28),
        (0x48, 0x64, 0x76),
        (0x50, 0x78, 0x6c),
        (0x7f, 0x66, 0x8f),
    }
    old_pixels = 0
    total = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            total += 1
            if image.getpixel((x, y)) in old_panel_colors:
                old_pixels += 1
    if total > 0 and old_pixels * 100 > total:
        raise RuntimeError(
            "desktop screenshot still resembles the old diagnostic-card layout "
            f"({old_pixels}/{total} panel-colored pixels)"
        )


def hidden_startup_flags():
    if os.name != "nt":
        return None, 0
    startupinfo = subprocess.STARTUPINFO()
    startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startupinfo.wShowWindow = 0
    return startupinfo, getattr(subprocess, "CREATE_NO_WINDOW", 0)


def main():
    parser = argparse.ArgumentParser(description="Capture srvros GUI presentation screenshots.")
    parser.add_argument("--root", default=str(default_root()))
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--out-dir", default="build/presentation-screens")
    parser.add_argument("--memory", default="512M")
    parser.add_argument("--display", default="none",
        help="QEMU display backend. Defaults to none so captures do not open a QEMU window.")
    parser.add_argument("--boot-wait", type=float, default=120)
    parser.add_argument("--shell-wait", type=float, default=30)
    parser.add_argument("--displayd-wait", type=float, default=45)
    parser.add_argument("--map-wait", type=float, default=15)
    parser.add_argument("--key-delay", type=float, default=0.025)
    parser.add_argument("--settle", type=float, default=0.6)
    parser.add_argument("--no-contact-sheet", action="store_true")
    parser.add_argument("--skip-assertions", action="store_true")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    iso = Path(args.iso)
    if not iso.is_absolute():
        iso = root / iso
    source_disk = Path(args.disk)
    if not source_disk.is_absolute():
        source_disk = root / source_disk
    out_dir = Path(args.out_dir)
    if not out_dir.is_absolute():
        out_dir = root / out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    for path in (r"C:\msys64\ucrt64\bin", r"C:\msys64\usr\bin"):
        if os.path.isdir(path):
            env["PATH"] = path + os.pathsep + env.get("PATH", "")
    qemu = shutil.which(args.qemu, path=env.get("PATH", "")) or args.qemu

    serial_port = choose_port(25000, 29000)
    qmp_port = choose_port(45000, 49000)
    captured = []

    with tempfile.TemporaryDirectory(prefix="srvros-presentation-") as temp_dir:
        disk = Path(temp_dir) / "srvros.exfat"
        shutil.copyfile(source_disk, disk)
        command = [
            qemu,
            "-M", "q35",
            "-m", args.memory,
            "-cdrom", str(iso),
            "-boot", "d",
            "-serial", f"tcp:127.0.0.1:{serial_port},server,nowait",
            "-qmp", f"tcp:127.0.0.1:{qmp_port},server,nowait",
            "-drive", f"if=none,id=exfat,file={disk},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=exfat,bus=ahci.0",
            "-display", args.display,
            "-monitor", "none",
            "-no-reboot",
        ]

        startupinfo, creationflags = hidden_startup_flags()
        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
            startupinfo=startupinfo, creationflags=creationflags)
        try:
            serial = connect_tcp(serial_port, 15, "serial")
            serial.settimeout(0.25)
            qmp = connect_tcp(qmp_port, 20, "qmp")
            qmp.settimeout(2)
            qmp.recv(4096)
            qmp_command(qmp, {"execute": "qmp_capabilities"})

            read_until_any(serial, [b"srv> "], args.boot_wait)
            path = out_dir / "01-console-monitor.png"
            capture_screen(qmp, path)
            captured.append(path)

            send_qmp_text(qmp, "run /fat/bin/sh\n", args.key_delay)
            read_until_any(serial, [b" $ ", b"srvsh: userspace shell ready"], args.shell_wait)
            read_for(serial, args.settle)
            path = out_dir / "02-shell-prompt.png"
            capture_screen(qmp, path)
            captured.append(path)

            send_qmp_text(qmp, "gui\n", args.key_delay)
            read_until_any(serial, [b"displayd: root backbuffer ready"], args.displayd_wait)
            read_for(serial, args.settle)
            path = out_dir / "03-empty-desktop.png"
            capture_screen(qmp, path)
            if not args.skip_assertions:
                assert_clean_desktop(path)
            captured.append(path)

            mouse = Mouse(qmp)
            for name, x, y, marker in LAUNCH_SEQUENCE:
                mouse.click(x, y)
                try:
                    read_until_any(serial, [marker], args.map_wait)
                except RuntimeError as exc:
                    print(f"presentation-screens: warning: {exc}", file=sys.stderr)
                read_for(serial, args.settle)
                path = out_dir / name
                capture_screen(qmp, path)
                captured.append(path)

            path = out_dir / "11-all-apps-open.png"
            capture_screen(qmp, path)
            captured.append(path)
            if not args.no_contact_sheet:
                sheet = make_contact_sheet(captured, out_dir / "contact-sheet.png")
                if sheet is not None:
                    print(f"presentation-screens: contact sheet {sheet}")
            for path in captured:
                print(f"presentation-screens: captured {path}")
        finally:
            try:
                process.terminate()
                process.wait(timeout=5)
            except Exception:
                process.kill()


if __name__ == "__main__":
    main()
