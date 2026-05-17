#!/usr/bin/env python3
import argparse
import os
import random
import re
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
            chunk = sock.recv(4096)
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


def send_command(sock, command, marker, timeout):
    sock.sendall(command.encode("ascii") + b"\n")
    return read_until(sock, marker.encode("ascii"), timeout)


def poll_command(sock, command, marker, timeout, interval=0.5):
    data = b""
    deadline = time.time() + timeout
    marker_bytes = marker.encode("ascii")
    while marker_bytes not in data and time.time() < deadline:
        sock.sendall(command.encode("ascii") + b"\n")
        data += read_until(sock, marker_bytes, min(1.0, max(0.1, deadline - time.time())))
        if marker_bytes in data:
            break
        time.sleep(interval)
    return data


def webd_pid_from(text):
    matches = re.findall(r"webd background pid ([0-9]+)", text)
    return matches[-1] if matches else None


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def main():
    parser = argparse.ArgumentParser(description="Verify srvros init/svscan service lifecycle controls.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=25)
    parser.add_argument("--shell-wait", type=float, default=3)
    parser.add_argument("--service-wait", type=float, default=10)
    parser.add_argument("--settle-wait", type=float, default=2)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    serial_port = random.randint(24000, 29000)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-service-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-service.exfat")
        shutil.copyfile(source_disk, disk)
        command = [
            args.qemu,
            "-M", "q35",
            "-m", args.memory,
            "-cdrom", iso,
            "-boot", "d",
            "-serial", f"tcp:127.0.0.1:{serial_port},server,nowait",
            "-drive", f"if=none,id=exfat,file={disk},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=exfat,bus=ahci.0",
            "-netdev", "user,id=net0",
            "-device", "e1000,netdev=net0",
            "-monitor", "none",
            "-no-reboot",
        ]

        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            sock = connect_serial(serial_port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            sock.sendall(b"run /fat/bin/sh\n")
            output += read_until(sock, b"srvsh: interactive shell", args.shell_wait)
            output += send_command(sock, "cat /fat/var/log/init.log", "init-script-ok", args.service_wait)
            output += poll_command(sock, "service webd status", "webd background pid", args.service_wait)
            output += send_command(sock, "service list", "enabled=true", args.service_wait)
            output += send_command(sock, "service disable webd", "webd disabled", args.service_wait)
            output += send_command(sock, "service webd status", "enabled=false", args.service_wait)
            output += send_command(sock, "service webd stop", "stopped pid", args.service_wait)
            output += read_for(sock, args.settle_wait)
            output += send_command(sock, "service webd status", "webd stopped enabled=false", args.service_wait)
            output += send_command(sock, "service reload", "service: reload requested", args.service_wait)
            output += read_for(sock, args.settle_wait)
            output += send_command(sock, "service webd status", "webd stopped enabled=false", args.service_wait)
            output += send_command(sock, "service enable webd", "webd enabled", args.service_wait)
            status_output = poll_command(sock, "service webd status", "webd background pid", args.service_wait)
            output += status_output
            webd_pid = webd_pid_from(status_output.decode("utf-8", "replace"))
            if webd_pid:
                output += send_command(sock, f"kill {webd_pid}; echo killed-webd", "killed-webd", args.service_wait)
                output += poll_command(sock, "netstat", "LISTEN", args.service_wait)
                output += send_command(sock, "ps", "PID STATE", args.service_wait)
            output += send_command(sock, "cat /fat/var/log/svscan.log", "svscan: webd restarting", args.service_wait)
            output += send_command(sock, "netstat", "LISTEN", args.service_wait)
            output += read_for(sock, 1)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)

    expected = [
        "init: started pid=",
        "init: startup complete",
        "init-script-ok",
        "init: /fat/etc/init.sh status 0",
        "svscan: started",
        "webd background pid",
        "enabled=true",
        "webd disabled",
        "enabled=false",
        "webd stopped enabled=false",
        "service: reload requested",
        "svscan: reload requested",
        "webd enabled",
        "svscan: webd restarting",
        "svscan: webd reaped",
        "Proto State",
        "10.0.2.15:80",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("service-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("service-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("service-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
