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


def qmp_command(sock, command):
    sock.sendall(json.dumps(command).encode("ascii") + b"\r\n")
    data = b""
    while b"\r\n" not in data:
        data += sock.recv(4096)
    return data


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


def hmp(sock, command):
    qmp_command(sock, {
        "execute": "human-monitor-command",
        "arguments": {"command-line": command},
    })


class Mouse:
    def __init__(self, qmp, x=96, y=96):
        self.qmp = qmp
        self.x = x
        self.y = y

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
            time.sleep(0.05)
        time.sleep(0.15)

    def button(self, down):
        hmp(self.qmp, "mouse_button 1" if down else "mouse_button 0")
        time.sleep(0.15)

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
    parser = argparse.ArgumentParser(description="Run a srvros displayd paint launch smoke test.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=80)
    parser.add_argument("--shell-wait", type=float, default=2)
    parser.add_argument("--ready-wait", type=float, default=6)
    parser.add_argument("--map-wait", type=float, default=12)
    parser.add_argument("--action-wait", type=float, default=5)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    serial_port = choose_port(38001, 42000)
    qmp_port = choose_port(42001, 46000)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")
    qemu = shutil.which(args.qemu, path=env.get("PATH", "")) or args.qemu

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-displayd-paint-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-displayd-paint.exfat")
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
            serial.sendall(b"gui\n")
            output += read_until(serial, b"displayd: root backbuffer ready", args.ready_wait)

            mouse = Mouse(qmp)
            mouse.click(80, 161)
            output += read_until(serial, b"paint: configure", args.map_wait)
            output += read_for(serial, args.action_wait)
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
        "displayd: root backbuffer ready",
        "displayd: launch PAINT /fat/bin/paint pid=",
        "displayd: mapped surface window PAINT",
        "paint: configure",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("displayd-paint-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("displayd-paint-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3
    print("displayd-paint-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
