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
        data += read_for(sock, 0.4)
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


def send_payload(sock, payload, delay):
    for byte in payload:
        sock.sendall(bytes([byte]))
        if delay > 0:
            time.sleep(delay)


def send_and_wait(sock, output, payload, marker, seconds, delay):
    send_payload(sock, payload, delay)
    output += read_until(sock, marker, seconds)
    output += read_until(sock, b" $ ", 3)
    return output


def main():
    parser = argparse.ArgumentParser(description="Exercise srvsh interactive line editing over serial.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=25)
    parser.add_argument("--command-wait", type=float, default=8)
    parser.add_argument("--send-delay", type=float, default=0.003)
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
    with tempfile.TemporaryDirectory(prefix="srvros-shell-edit-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-shell-edit.exfat")
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
            "-monitor", "none",
            "-no-reboot",
        ]
        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            sock = connect_serial(serial_port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            send_payload(sock, b"run /fat/bin/sh\n", args.send_delay)
            output += read_until(sock, b" $ ", 6)
            output = send_and_wait(sock, output, b"echo wrng\x15echo ctrl-u-ok\n", b"ctrl-u-ok", args.command_wait, args.send_delay)
            output = send_and_wait(sock, output, b"echo ac\x1b[Db\n", b"abc", args.command_wait, args.send_delay)
            output = send_and_wait(sock, output, b"echo yank-target\x15\x19\n", b"yank-target", args.command_wait, args.send_delay)
            output = send_and_wait(sock, output, b"XYZ\x01echo home-\x05-end\n", b"home-XYZ-end", args.command_wait, args.send_delay)
            output = send_and_wait(sock, output, b"echo hist-saved\n", b"hist-saved", args.command_wait, args.send_delay)
            output = send_and_wait(sock, output, b"echo draft\x1b[A\x1b[B\n", b"draft", args.command_wait, args.send_delay)
            output = send_and_wait(sock, output, b"echo word-one word-two\x17\n", b"word-one", args.command_wait, args.send_delay)
            output += read_for(sock, 1)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)

    markers = [
        "ctrl-u-ok",
        "abc",
        "yank-target",
        "home-XYZ-end",
        "hist-saved",
        "draft",
        "word-one",
    ]
    missing = [marker for marker in markers if marker not in text]
    if "wrngecho ctrl-u-ok" in text:
        missing.append("ctrl-u cleared prior text")
    if "draftA" in text or "draftA[B" in text:
        missing.append("history down restored the draft")
    if has_fatal_exception(text):
        print("shell-edit-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("shell-edit-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("shell-edit-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
