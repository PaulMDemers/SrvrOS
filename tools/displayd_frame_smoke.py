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
            time.sleep(0.1)
        time.sleep(0.15)

    def button(self, down):
        hmp(self.qmp, "mouse_button 1" if down else "mouse_button 0")
        time.sleep(0.15)

    def click(self, x, y):
        self.move_to(x, y)
        self.button(True)
        self.button(False)

    def drag(self, from_x, from_y, to_x, to_y):
        self.move_to(from_x, from_y)
        self.button(True)
        time.sleep(0.25)
        self.move_to(to_x, to_y)
        self.button(False)


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def stop_process(process):
    if process.poll() is not None:
        return
    try:
        process.terminate()
        process.wait(timeout=3)
        return
    except Exception:
        pass
    if os.name == "nt":
        try:
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, timeout=5)
            process.wait(timeout=3)
            return
        except Exception:
            pass
    try:
        process.kill()
    except Exception:
        pass
    try:
        process.wait(timeout=5)
    except Exception:
        pass


def main():
    parser = argparse.ArgumentParser(description="Run a srvros displayd frame-control smoke test.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=80)
    parser.add_argument("--shell-wait", type=float, default=2)
    parser.add_argument("--map-wait", type=float, default=8)
    parser.add_argument("--action-wait", type=float, default=5)
    parser.add_argument("--final-wait", type=float, default=45)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    serial_port = choose_port(30001, 34000)
    qmp_port = choose_port(34001, 38000)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")
    qemu = shutil.which(args.qemu, path=env.get("PATH", "")) or args.qemu

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-displayd-frame-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-displayd-frame.exfat")
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
            serial.sendall(b"gui --frame-smoke-autostart\n")
            output += read_until(serial, b"displayd: mapped surface window NOTES", args.map_wait)

            mouse = Mouse(qmp)
            # Notes starts at 478,288 after work-area-aware placement.
            mouse.click(504, 294)
            output += read_until(serial, b"displayd: minimize NOTES state=1", args.action_wait)
            # Notes taskbar entry is the third task button at 1280x800.
            mouse.click(760, 760)
            output += read_until(serial, b"displayd: taskbar restore NOTES", args.action_wait)
            # GUI2 Demo taskbar entry is the second task button.
            mouse.click(560, 760)
            output += read_until(serial, b"displayd: taskbar focus GUI2 DEMO", args.action_wait)
            mouse.drag(560, 300, 202, 348)
            output += read_until(serial, b"displayd: drag NOTES", args.action_wait)
            # GUI2 demo frame starts at 620,240 and is 302x206. Drag the bottom-right grip.
            mouse.drag(914, 438, 974, 474)
            output += read_until(serial, b"displayd: resize GUI2 DEMO 360x216", args.action_wait)
            # GUI2 demo title frame starts at 620,240. Close button is near x=633,y=246.
            mouse.click(633, 246)
            output += read_until(serial, b"displayd: remove GUI2 DEMO reason=destroy", args.action_wait)
            # Relaunch GUI2 Demo from the dock and verify resize still works after cleanup.
            mouse.click(60, 195)
            output += read_until(serial, b"displayd: mapped surface window GUI2 DEMO", args.map_wait)
            # Dock-launched GUI2 windows are compositor-placed in the upper-right.
            mouse.drag(1254, 345, 1274, 381)
            output += read_until(serial, b"displayd: resize GUI2 DEMO", args.action_wait)
            # Exit button sits below the dock launchers at 1280x800.
            mouse.click(60, 303)
            output += read_until(serial, b"displayd: shutdown requested", args.action_wait)
            output += read_until(serial, b"displayd: shutdown complete", args.final_wait)
        finally:
            stop_process(process)

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)

    expected = [
        "gui: starting displayd",
        "displayd: mapped surface window SURFACE DEMO",
        "displayd: mapped surface window GUI2 DEMO",
        "displayd: mapped surface window NOTES",
        "displayd: minimize NOTES state=1",
        "displayd: taskbar restore NOTES",
        "displayd: taskbar focus GUI2 DEMO",
        "displayd: drag NOTES",
        "displayd: resize GUI2 DEMO 360x216",
        "gui2demo: configure 360x216",
        "displayd: close GUI2 DEMO",
        "gui2demo: close",
        "displayd: remove GUI2 DEMO reason=destroy",
        "displayd: launch GUI2 /fat/bin/gui2demo pid=",
        "displayd: shutdown requested",
        "displayd: close NOTES",
        "displayd: shutdown complete",
        "displayd: exited",
    ]
    missing = [marker for marker in expected if marker not in text]
    if text.count("displayd: resize GUI2 DEMO") < 2:
        missing.append("displayd: resize GUI2 DEMO twice")
    if has_fatal_exception(text):
        print("displayd-frame-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("displayd-frame-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("displayd-frame-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
