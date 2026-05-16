#!/usr/bin/env python3
import argparse
import os
import random
import shutil
import socket
import subprocess
import sys
import tempfile
import time


HTTP_PATHS = [
    ("/", [b"HTTP/1.1 200 OK", b"<h1>srvros</h1>"]),
    ("/assets/site.css", [b"HTTP/1.1 200 OK", b"max-width:48rem"]),
    ("/status.txt", [b"HTTP/1.1 200 OK", b"static file serving"]),
    ("/large.txt", [b"HTTP/1.1 200 OK", b"srvros large tcp payload ends"]),
]


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


def http_get(port, path, timeout):
    deadline = time.time() + timeout
    request = f"GET {path} HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n".encode("ascii")
    last_error = None
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=2) as sock:
                sock.settimeout(3)
                sock.sendall(request)
                chunks = []
                while True:
                    try:
                        chunk = sock.recv(4096)
                    except socket.timeout:
                        break
                    if not chunk:
                        break
                    chunks.append(chunk)
                response = b"".join(chunks)
                if b"HTTP/1.1" in response:
                    return response
        except OSError as exc:
            last_error = exc
        time.sleep(0.2)
    raise RuntimeError(f"GET {path} failed: {last_error}")


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def send_command(sock, command, marker, timeout):
    sock.sendall(command.encode("ascii") + b"\n")
    return read_until(sock, marker.encode("ascii"), timeout)


def main():
    parser = argparse.ArgumentParser(description="Repeated srvros networking soak over e1000/QEMU user networking.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--boot-wait", type=float, default=25)
    parser.add_argument("--service-wait", type=float, default=12)
    parser.add_argument("--command-wait", type=float, default=60)
    parser.add_argument("--http-wait", type=float, default=25)
    parser.add_argument("--memory", default="512M")
    parser.add_argument("--hostfwd-target", default="10.0.2.15:80")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    serial_port = random.randint(24000, 29000)
    http_port = random.randint(18080, 18999)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    missing = []
    with tempfile.TemporaryDirectory(prefix="srvros-net-soak-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-net-soak.exfat")
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
            "-netdev", f"user,id=net0,hostfwd=tcp:127.0.0.1:{http_port}-{args.hostfwd_target}",
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
            sock.sendall(b"run /fat/bin/sh --login\n")
            output += read_until(sock, b"webd started pid", args.service_wait)
            output += send_command(sock, "service webd status", "webd background pid", args.service_wait)

            for round_index in range(args.rounds):
                for path, markers in HTTP_PATHS:
                    try:
                        response = http_get(http_port, path, args.http_wait)
                    except RuntimeError as exc:
                        missing.append(str(exc))
                        continue
                    for marker in markers:
                        if marker not in response:
                            missing.append(f"round {round_index + 1} {path} missing {marker.decode('ascii')}")

                output += send_command(sock, "netcheck", "netcheck: ok", args.command_wait)
                output += send_command(sock, "netstat", "Proto State", args.service_wait)
                output += read_for(sock, 0.5)
                output += send_command(sock, "ifconfig", "rx frames", args.service_wait)
                output += read_for(sock, 0.5)
                output += send_command(sock, "arp", "HWaddress", args.service_wait)
                output += read_for(sock, 0.5)
            output += send_command(sock, "cat /fat/var/log/webd.log", "webd: serving", args.service_wait)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)

    for marker in [
        "webd started pid",
        "webd: serving /fat/www on 10.0.2.15:80",
        "webd background pid",
        "log /fat/var/log/webd.log",
        "netcheck: ok",
        "10.0.2.15:80",
        "e1000: flags=UP,RUNNING",
        "52:55:0a:00:02:",
    ]:
        if marker not in text:
            missing.append(marker)
    if "netcheck: fail" in text:
        missing.append("netcheck failure")
    if has_fatal_exception(text):
        print("net-soak: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("net-soak: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print(f"net-soak: ok rounds={args.rounds}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
