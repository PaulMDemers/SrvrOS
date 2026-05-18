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


def send_serial(sock, data, delay):
    if isinstance(data, str):
        data = data.encode("ascii")
    if delay <= 0:
        sock.sendall(data)
        return
    for byte in data:
        sock.sendall(bytes([byte]))
        time.sleep(delay)


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
    parser = argparse.ArgumentParser(description="Exercise srvsh linenoise editing over the serial console.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--line-wait", type=float, default=4)
    parser.add_argument("--send-delay", type=float, default=0.001)
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
        env["PATH"] = msys_usr + os.pathsep + msys_ucrt + os.pathsep + env.get("PATH", "")

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-shell-edit-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-shell-edit.exfat")
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
            send_serial(sock, "run /fat/bin/sh\n", args.send_delay)
            output += read_until(sock, b" $ ", 6)

            commands = [
                b"ech\t completion-ok\n",
                b"write /fat/completion-alpha prefix-alpha\n",
                b"write /fat/completion-alpine prefix-alpine\n",
                b"cat /fat/completion-a\tha\n",
                b"echo wrng" + bytes([21]) + b"echo ctrl-u-ok\n",
                b"echo ac" + bytes([27]) + b"[Db\n",
                b"echo yank-target" + bytes([21]) + bytes([25]) + b"\n",
                b"XYZ" + bytes([1]) + b"echo home-" + bytes([5]) + b"-end\n",
                b" wrong",
                bytes([1]) + b"echo" + bytes([5]) + b"-ok\n",
                b"echo discard-this" + bytes([21]) + b"echo ctrl-u-ok\n",
                b"echo hist-ok\n",
                bytes([27]) + b"[A\n",
                b"echo draft" + bytes([27]) + b"[A" + bytes([27]) + b"[B\n",
                b"echo word-one word-two" + bytes([23]) + b"\n",
                b"history -c\n",
                b"export HISTFILE=/fat/hist-smoke.txt\n",
                b"export HISTSIZE=3\n",
                b"echo hist-one\n",
                b"echo hist-two\n",
                b"history 3\n",
                b"history -w\n",
                b"grep hist-two /fat/hist-smoke.txt\n",
                b"write /fat/bad.sh 'echo diag-ok'\n",
                b"write -a /fat/bad.sh missingcmd\n",
                b"sh /fat/bad.sh\n",
                b"rm /fat/completion-alpha /fat/completion-alpine\n",
                b"exit\n",
            ]
            for command_bytes in commands:
                send_serial(sock, command_bytes, args.send_delay)
                if command_bytes == b"exit\n":
                    output += read_until(sock, b"srv> ", 10)
                else:
                    output += read_until(sock, b" $ ", args.line_wait)
            send_serial(sock, "fsck /fat\n", args.send_delay)
            output += read_until(sock, b"srv> ", 15)
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
        "completion-ok",
        "prefix-alpha",
        "abc",
        "yank-target",
        "home-XYZ-end",
        "wrong-ok",
        "ctrl-u-ok",
        "draft",
        "word-one",
        "history 3",
        "hist-two",
        "/fat/bad.sh:2: sh: command not found: missingcmd",
        "exfat-check:",
        "errors=0 ok",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("shell-edit-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if text.count("hist-ok") < 2:
        print("shell-edit-smoke: history recall did not repeat command", file=sys.stderr)
        return 3
    if missing:
        print("shell-edit-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 4

    print("shell-edit-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
