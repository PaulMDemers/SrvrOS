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


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def main():
    parser = argparse.ArgumentParser(description="Verify srvros staged ports in QEMU.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--line-wait", type=float, default=4)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    port = random.randint(24000, 29000)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-ports-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-ports.exfat")
        shutil.copyfile(source_disk, disk)
        command = [
            args.qemu,
            "-M", "q35",
            "-m", args.memory,
            "-cdrom", iso,
            "-boot", "d",
            "-serial", f"tcp:127.0.0.1:{port},server,nowait",
            "-drive", f"if=none,id=exfat,file={disk},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=exfat,bus=ahci.0",
            "-monitor", "none",
            "-no-reboot",
        ]
        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            sock = connect_serial(port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            sock.sendall(b"run /fat/bin/sh\n")
            output += read_until(sock, b" $ ", 5)
            for line in ["zlibdemo\n", "jsondemo\n", "inidemo\n", "linedemo\n", "posixdemo\n", "exit\n"]:
                sock.sendall(line.encode("ascii"))
                output += read_until(sock, b"srv> " if line.strip() == "exit" else b" $ ", args.line_wait)
            sock.sendall(b"fsck /fat\n")
            output += read_until(sock, b"srv> ", 10)
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
        "zlibdemo: compressed",
        "zlibdemo: restored",
        "zlibdemo: ok zlib 1.3.2",
        "jsondemo: parse ok",
        "jsondemo: roundtrip ok",
        "jsondemo: ok cJSON 1.7.19",
        "inidemo: string parse ok",
        "inidemo: file parse ok",
        "inidemo: ok inih r62",
        "linedemo: history ok",
        "linedemo: ok linenoise 2.0",
        "posixdemo: fstat-size=",
        "posixdemo: dup ok",
        "posixdemo: stdio ok",
        "posixdemo: rw ok",
        "posixdemo: dup write ok",
        "posixdemo: fs api ok",
        "posixdemo: nonblock ok",
        "posixdemo: poll ok",
        "posixdemo: pipe ok",
        "posixdemo: sbrk ok",
        "posixdemo: stdlib extra ok",
        "posixdemo: math ok",
        "posixdemo: pread ok",
        "posixdemo: posix misc ok",
        "posixdemo: spawn ok",
        "posixdemo: execve ok",
        "posixdemo: cloexec ok",
        "posixdemo: ok",
        "exfat-check:",
        "errors=0 ok",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("ports-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("ports-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("ports-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
