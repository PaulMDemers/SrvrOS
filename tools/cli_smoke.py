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
    parser = argparse.ArgumentParser(description="Run a srvros shell CLI serial smoke test.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--shell-wait", type=float, default=2)
    parser.add_argument("--after-wait", type=float, default=10)
    parser.add_argument("--line-wait", type=float, default=0.7)
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
    script = (
        "echo shell-ok\n"
        "ls /fat/bin\n"
        "cat /fat/status.txt\n"
        "wc /fat/status.txt\n"
        "stat /fat/status.txt\n"
        "head -n 1 /fat/status.txt\n"
        "grep exFAT /fat/status.txt\n"
        "cat /fat/status.txt > /fat/cat-redir.txt\n"
        "cat /fat/status.txt >> /fat/cat-redir.txt\n"
        "stat /fat/cat-redir.txt\n"
        "wc /fat/cat-redir.txt\n"
        "ls /fat/bin > /fat/bin-list.txt\n"
        "grep webd /fat/bin-list.txt\n"
        "echo \"two  spaces\"; echo semi-ok\n"
        "cat /fat/etc/init.sh\n"
        "echo redirected > /fat/redir.txt\n"
        "echo appended >> /fat/redir.txt\n"
        "cat /fat/redir.txt\n"
        "write /fat/boot.sh \"echo script-ok\"\n"
        "write -a /fat/boot.sh \"echo appended-script-ok\"\n"
        "source /fat/boot.sh\n"
        "cp /fat/status.txt /fat/status-copy.txt\n"
        "stat /fat/status-copy.txt\n"
        "cp /fat/redir.txt /fat/redir-copy.txt\n"
        "cat /fat/redir-copy.txt\n"
        "cp /fat/bin/sh /fat/sh-copy\n"
        "stat /fat/sh-copy\n"
        "rm /fat/sh-copy\n"
        "ps\n"
        "write /fat/shell.txt hello-from-sh\n"
        "cat /fat/shell.txt\n"
        "write /fat/move-src.txt move-me\n"
        "mv /fat/move-src.txt /fat/move-dst.txt\n"
        "cat /fat/move-dst.txt\n"
        "stat /fat/move-src.txt\n"
        "rm /fat/move-dst.txt\n"
        "rm /fat/shell.txt\n"
        "stat /fat/shell.txt\n"
        "write /fat/shell.txt hello-again\n"
        "cat /fat/shell.txt\n"
        "rm /fat/shell.txt\n"
        "stat /fat/shell.txt\n"
        "posixdemo\n"
        "exit\n"
    )
    with tempfile.TemporaryDirectory(prefix="srvros-cli-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-cli.exfat")
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
            output += read_until(sock, b" $ ", args.shell_wait)
            lines = script.splitlines(True)
            for line in lines:
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

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)

    expected = [
        "srvsh: interactive shell",
        "shell-ok",
        "sh",
        "status.txt",
        "srvros webd: static file serving from exFAT is online.",
        "/fat/status.txt: 55 bytes",
        "/fat/cat-redir.txt: 110 bytes",
        "2 18 110 /fat/cat-redir.txt",
        "webd",
        "two  spaces",
        "semi-ok",
        "init-script-ok",
        "redirected",
        "appended",
        "script-ok",
        "appended-script-ok",
        "/fat/status-copy.txt: 55 bytes",
        "/fat/sh-copy:",
        "PID STATE",
        "/fat/bin/sh",
        "hello-from-sh",
        "move-me",
        "stat: not found: /fat/move-src.txt",
        "hello-again",
        "stat: not found: /fat/shell.txt",
        "posixdemo: start pid=",
        "posixdemo: read=hello from posix",
        "posixdemo: malloc ok",
        "posixdemo: socket bind ok",
        "posixdemo: ok",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("cli-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("cli-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("cli-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
