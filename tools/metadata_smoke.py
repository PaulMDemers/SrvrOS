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


def run_boot(args, disk, script):
    port = random.randint(24000, 29000)
    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    command = [
        args.qemu,
        "-M", "q35",
        "-m", args.memory,
        "-cdrom", args.iso,
        "-boot", "d",
        "-serial", f"tcp:127.0.0.1:{port},server,nowait",
        "-drive", f"if=none,id=exfat,file={disk},format=raw",
        "-device", "ich9-ahci,id=ahci",
        "-device", "ide-hd,drive=exfat,bus=ahci.0",
        "-monitor", "none",
        "-no-reboot",
    ]

    output = b""
    process = subprocess.Popen(command, cwd=args.root, env=env,
        stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    try:
        sock = connect_serial(port, 15)
        sock.settimeout(0.3)
        output += read_until(sock, b"srv> ", args.boot_wait)
        sock.sendall(b"run /fat/bin/sh\n")
        output += read_until(sock, b" $ ", args.shell_wait)
        for line in script.splitlines(True):
            sock.sendall(line.encode("ascii"))
            if line.strip() == "exit":
                output += read_for(sock, args.line_wait)
            else:
                output += read_until(sock, b" $ ", max(args.line_wait, 2.0))
        output += read_for(sock, args.after_wait)
    finally:
        try:
            process.terminate()
            process.wait(timeout=3)
        except Exception:
            process.kill()
    return output.decode("utf-8", "replace")


def main():
    parser = argparse.ArgumentParser(description="Verify exFAT metadata sidecar survives a reboot.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--shell-wait", type=float, default=2)
    parser.add_argument("--after-wait", type=float, default=2)
    parser.add_argument("--line-wait", type=float, default=0.7)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    args.root = os.path.abspath(args.root)
    args.iso = args.iso if os.path.isabs(args.iso) else os.path.join(args.root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(args.root, args.disk)

    first_script = (
        "write /fat/persist.txt persistent-metadata\n"
        "chmod 600 /fat/persist.txt\n"
        "mkdir /fat/persistdir\n"
        "chmod 700 /fat/persistdir\n"
        "stat /fat/persist.txt\n"
        "stat /fat/persistdir\n"
        "ls /fat/.srvros\n"
        "exit\n"
    )
    second_script = (
        "cat /fat/persist.txt\n"
        "stat /fat/persist.txt\n"
        "stat /fat/persistdir\n"
        "ls /fat/.srvros\n"
        "exit\n"
    )

    with tempfile.TemporaryDirectory(prefix="srvros-metadata-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-metadata.exfat")
        shutil.copyfile(source_disk, disk)
        first = run_boot(args, disk, first_script)
        second = run_boot(args, disk, second_script)

    text = first + "\n--- reboot ---\n" + second
    sys.stdout.write(text)

    expected = [
        "/fat/persist.txt: 20 bytes file mode 600",
        "/fat/persistdir: 0 bytes directory mode 700",
        "meta",
        "persistent-metadata",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("metadata-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("metadata-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3
    print("metadata-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
