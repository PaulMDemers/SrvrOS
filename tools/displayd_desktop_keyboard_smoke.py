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


def send_key(sock, key, delay):
    hmp(sock, f"sendkey {key}")
    time.sleep(delay)


class Mouse:
    def __init__(self, qmp, x=96, y=96):
        self.qmp = qmp
        for _ in range(40):
            hmp(self.qmp, "mouse_move -60 -60")
            time.sleep(0.01)
        time.sleep(0.2)
        self.x = 0
        self.y = 0

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


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def main():
    parser = argparse.ArgumentParser(
        description="Run a srvros displayd desktop keyboard smoke test.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=80)
    parser.add_argument("--shell-wait", type=float, default=2)
    parser.add_argument("--map-wait", type=float, default=10)
    parser.add_argument("--action-wait", type=float, default=8)
    parser.add_argument("--key-delay", type=float, default=0.2)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    serial_port = choose_port(50001, 54000)
    qmp_port = choose_port(54001, 56000)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")
    qemu = shutil.which(args.qemu, path=env.get("PATH", "")) or args.qemu

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-displayd-desktop-keyboard-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-displayd-desktop-keyboard.exfat")
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
            serial.sendall(b"gui --frame-keyboard-autostart\n")
            output += read_until(serial, b"displayd: mapped surface window NOTES", args.map_wait)

            mouse = Mouse(qmp)
            mouse.click(504, 294)
            output += read_until(serial, b"displayd: minimize NOTES state=1", args.action_wait)

            mouse.click(1240, 120)
            send_key(qmp, "shift-tab", args.key_delay)
            output += read_until(serial, b"displayd: desktop focus taskbar NOTES", args.action_wait)
            send_key(qmp, "ret", args.key_delay)
            output += read_until(serial, b"displayd: taskbar restore NOTES", args.action_wait)

            mouse.click(1240, 120)
            send_key(qmp, "shift-tab", args.key_delay)
            output += read_until(serial, b"displayd: desktop focus taskbar NOTES", args.action_wait)
            send_key(qmp, "ret", args.key_delay)
            output += read_until(serial, b"displayd: taskbar focus NOTES", args.action_wait)

            mouse.click(1240, 120)
            for _ in range(5):
                send_key(qmp, "tab", args.key_delay)
            output += read_until(serial, b"displayd: desktop focus launcher GUI2", args.action_wait)
            send_key(qmp, "ret", args.key_delay)
            output += read_until(serial, b"displayd: launch GUI2 /fat/bin/gui2demo pid=", args.action_wait)

            mouse.click(1240, 120)
            for _ in range(8):
                send_key(qmp, "tab", args.key_delay)
            output += read_until(serial, b"displayd: desktop focus exit", args.action_wait)
            send_key(qmp, "ret", args.key_delay)
            output += read_until(serial, b"displayd: shutdown requested", args.action_wait)
            output += read_until(serial, b"displayd: shutdown complete", args.action_wait * 4)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)

    expected = [
        "gui: starting displayd",
        "displayd: mapped surface window SURFACE DEMO",
        "displayd: mapped surface window GUI2 DEMO",
        "displayd: mapped surface window NOTES",
        "displayd: minimize NOTES state=1",
        "displayd: desktop focus taskbar NOTES",
        "displayd: taskbar restore NOTES",
        "displayd: taskbar focus NOTES",
        "displayd: desktop focus launcher GUI2",
        "displayd: launch GUI2 /fat/bin/gui2demo pid=",
        "displayd: desktop focus exit",
        "displayd: shutdown requested",
        "displayd: shutdown complete",
        "displayd: exited",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("displayd-desktop-keyboard-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("displayd-desktop-keyboard-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("displayd-desktop-keyboard-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
