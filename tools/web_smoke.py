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
    last_response = b""
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=2) as sock:
                sock.settimeout(2)
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
                last_response = b"".join(chunks)
                if b"HTTP/1.1" in last_response:
                    return last_response
        except OSError as exc:
            last_error = exc
        time.sleep(0.5)
    if last_response:
        return last_response
    raise RuntimeError(f"http request failed: {last_error}")


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def main():
    parser = argparse.ArgumentParser(description="Boot srvros with e1000 and verify webd over host forwarding.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--shell-wait", type=float, default=2)
    parser.add_argument("--service-wait", type=float, default=8)
    parser.add_argument("--settle-wait", type=float, default=1)
    parser.add_argument("--http-wait", type=float, default=25)
    parser.add_argument("--memory", default="512M")
    parser.add_argument("--hostfwd-target", default="10.0.2.15:80")
    parser.add_argument("--pcap", default="")
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
    response = b""
    http_error = None
    with tempfile.TemporaryDirectory(prefix="srvros-web-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-web.exfat")
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
        if args.pcap:
            pcap = args.pcap if os.path.isabs(args.pcap) else os.path.join(root, args.pcap)
            command.extend(["-object", f"filter-dump,id=webdump,netdev=net0,file={pcap}"])

        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            sock = connect_serial(serial_port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            sock.sendall(b"run /fat/bin/sh --login\n")
            output += read_until(sock, b"srvsh: interactive shell", args.shell_wait)
            output += read_until(sock, b"webd: serving", args.service_wait)
            sock.sendall(b"service webd status\n")
            output += read_until(sock, b"webd background pid", args.service_wait)
            output += read_for(sock, args.settle_wait)
            try:
                response = http_get(http_port, "/", args.http_wait)
            except RuntimeError as exc:
                http_error = exc
            output += read_for(sock, 3)
            if http_error is not None or b"HTTP/1.1" not in response:
                sock.sendall(b"exit\n")
                output += read_until(sock, b"srv> ", 5)
                sock.sendall(b"net\n")
                output += read_until(sock, b"srv> ", 5)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)
    sys.stdout.write(response.decode("utf-8", "replace"))

    expected_serial = [
        "e1000:",
        "net: static ipv4=10.0.2.15",
        "init-script-ok",
        "webd: serving /fat/www on 10.0.2.15:80",
        "webd background pid",
    ]
    expected_response = [
        b"HTTP/1.1 200 OK",
        b"<h1>srvros</h1>",
        b"ring-3 web server",
    ]
    missing = [marker for marker in expected_serial if marker not in text]
    missing += [marker.decode("ascii") for marker in expected_response if marker not in response]
    if http_error is not None:
        missing.append(str(http_error))
    if has_fatal_exception(text):
        print("web-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("web-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("web-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
