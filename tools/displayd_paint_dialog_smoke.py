#!/usr/bin/env python3
import argparse
import json
import os
import random
import shutil
import socket
import subprocess
import sys
import tempfile
import time


def read_for(sock, seconds):
    chunks = []
    deadline = time.time() + seconds
    while time.time() < deadline:
        try:
            chunk = sock.recv(8192)
            if not chunk:
                break
            chunks.append(chunk)
        except socket.timeout:
            pass
    return b"".join(chunks)


def read_until(sock, marker, seconds):
    data = b""
    deadline = time.time() + seconds
    while marker not in data and time.time() < deadline:
        data += read_for(sock, 0.5)
    return data


def connect_serial(port, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1)
        except OSError:
            time.sleep(0.2)
    raise RuntimeError("serial connection failed")


def connect_qmp(port, timeout):
    deadline = time.time() + timeout
    last_error = None
    while time.time() < deadline:
        try:
            sock = socket.create_connection(("127.0.0.1", port), timeout=1)
            sock.settimeout(1)
            sock.recv(4096)
            qmp_command(sock, {"execute": "qmp_capabilities"})
            return sock
        except OSError as exc:
            last_error = exc
            time.sleep(0.2)
    raise RuntimeError(f"qmp connection failed: {last_error}")


def choose_port(start, end):
    for _ in range(200):
        port = random.randint(start, end)
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            try:
                probe.bind(("127.0.0.1", port))
                return port
            except OSError:
                pass
    raise RuntimeError("no free local port found")


def qmp_command(sock, command):
    sock.sendall(json.dumps(command).encode("ascii") + b"\r\n")
    data = b""
    while b"\r\n" not in data:
        data += sock.recv(4096)
    return data


def hmp(sock, command):
    qmp_command(sock, {
        "execute": "human-monitor-command",
        "arguments": {
            "command-line": command,
        },
    })


def send_qmp_key(sock, key):
    for down in (True, False):
        qmp_command(sock, {
            "execute": "input-send-event",
            "arguments": {
                "events": [{
                    "type": "key",
                    "data": {
                        "down": down,
                        "key": {
                            "type": "qcode",
                            "data": key,
                        },
                    },
                }],
            },
        })


def send_qmp_text(sock, text, key_delay):
    key_names = {
        " ": "spc",
        "\n": "ret",
        "\r": "ret",
        "-": "minus",
        "=": "equal",
        ".": "dot",
        "/": "slash",
        "\\": "backslash",
        "'": "apostrophe",
        ";": "semicolon",
        ",": "comma",
    }
    for char in text:
        key = key_names.get(char, char.lower())
        if len(key) != 1 and key not in key_names.values():
            continue
        send_qmp_key(sock, key)
        time.sleep(key_delay)


class Mouse:
    def __init__(self, qmp, x=96, y=96):
        self.qmp = qmp
        self.x = x
        self.y = y

    def move_to(self, x, y):
        dx = x - self.x
        dy = y - self.y
        while dx != 0 or dy != 0:
            step_x = max(-60, min(60, dx))
            step_y = max(-60, min(60, dy))
            hmp(self.qmp, f"mouse_move {step_x} {step_y}")
            self.x += step_x
            self.y += step_y
            dx = x - self.x
            dy = y - self.y
            time.sleep(0.08)
        time.sleep(0.12)

    def button(self, down):
        hmp(self.qmp, "mouse_button 1" if down else "mouse_button 0")
        time.sleep(0.12)

    def click(self, x, y):
        self.move_to(x, y)
        self.button(True)
        self.button(False)

    def drag(self, from_x, from_y, to_x, to_y):
        self.move_to(from_x, from_y)
        self.button(True)
        time.sleep(0.15)
        self.move_to(to_x, to_y)
        self.button(False)


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def run_shell(serial, command, wait=2):
    serial.sendall(command.encode("ascii") + b"\n")
    return read_until(serial, b" $ ", wait)


def main():
    parser = argparse.ArgumentParser(
        description="Run a srvros Paint shared BMP file-dialog QMP smoke test.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=80)
    parser.add_argument("--shell-wait", type=float, default=2)
    parser.add_argument("--ready-wait", type=float, default=8)
    parser.add_argument("--map-wait", type=float, default=10)
    parser.add_argument("--action-wait", type=float, default=8)
    parser.add_argument("--final-wait", type=float, default=40)
    parser.add_argument("--key-delay", type=float, default=0.04)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    serial_port = choose_port(54001, 58000)
    qmp_port = choose_port(58001, 62000)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")
    qemu = shutil.which(args.qemu, path=env.get("PATH", "")) or args.qemu

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-displayd-paint-dialog-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-displayd-paint-dialog.exfat")
        shutil.copyfile(source_disk, disk)
        command = [
            qemu,
            "-M", "q35",
            "-m", args.memory,
            "-cdrom", iso,
            "-boot", "d",
            "-serial", f"tcp:127.0.0.1:{serial_port},server,nowait",
            "-qmp", f"tcp:127.0.0.1:{qmp_port},server,nowait",
            "-drive", f"if=none,id=exfat,file={disk},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=exfat,bus=ahci.0",
            "-display", "none",
            "-monitor", "none",
            "-no-reboot",
        ]

        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            serial = connect_serial(serial_port, 15)
            serial.settimeout(0.3)
            qmp = connect_qmp(qmp_port, 20)
            output += read_until(serial, b"srv> ", args.boot_wait)
            serial.sendall(b"run /fat/bin/sh\n")
            output += read_until(serial, b" $ ", args.shell_wait)
            output += run_shell(serial, "mkdir -p /fat/home", args.shell_wait)
            serial.sendall(b"gui --paint-dialog-smoke\n")
            output += read_until(serial, b"displayd: root backbuffer ready",
                args.ready_wait)
            output += read_until(serial,
                b"displayd: launched paint-dialog pid=", args.map_wait)
            output += read_until(serial,
                b"displayd: mapped surface window PAINT", args.map_wait)
            output += read_until(serial, b"paint: configure", args.map_wait)

            mouse = Mouse(qmp)
            # Paint frame starts at (260,150); client origin is (261,175).
            mouse.drag(431, 245, 455, 269)
            output += read_until(serial, b"displayd: focus PAINT",
                args.action_wait)

            # Save As toolbar button, then the centered modal's SAVE button.
            mouse.click(681, 484)
            time.sleep(0.5)
            mouse.click(491, 492)
            output += read_until(serial, b"paint: save /fat/home/paint-dialog.bmp",
                args.action_wait)

            # Clear the current canvas so the following Open path has work to do.
            mouse.click(571, 484)
            output += read_until(serial, b"paint: clear", args.action_wait)

            # Open toolbar button, type the saved BMP basename, then accept.
            mouse.click(516, 484)
            time.sleep(0.5)
            mouse.click(385, 454)
            send_qmp_text(qmp, "paint-dialog.bmp", args.key_delay)
            mouse.click(491, 492)
            output += read_until(serial, b"paint: open /fat/home/paint-dialog.bmp",
                args.action_wait)

            mouse.click(273, 156)
            output += read_until(serial, b"paint: exited", args.action_wait)
            mouse.click(60, 303)
            output += read_until(serial, b"displayd: shutdown complete",
                args.final_wait)
            output += read_until(serial, b" $ ", args.shell_wait)
            serial.sendall(b"stat /fat/home/paint-dialog.bmp\n")
            output += read_until(serial, b"/fat/home/paint-dialog.bmp", args.action_wait)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)

    expected = [
        "displayd: root backbuffer ready",
        "displayd: mapped surface window PAINT",
        "paint: configure",
        "paint: save /fat/home/paint-dialog.bmp",
        "paint: clear",
        "paint: open /fat/home/paint-dialog.bmp",
        "paint: exited",
        "displayd: shutdown complete",
        "displayd: exited",
        "/fat/home/paint-dialog.bmp",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("displayd-paint-dialog-smoke: fatal exception detected",
            file=sys.stderr)
        return 2
    if missing:
        print("displayd-paint-dialog-smoke: missing markers:",
            file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("displayd-paint-dialog-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
