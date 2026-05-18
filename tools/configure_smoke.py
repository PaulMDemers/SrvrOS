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


def send_serial(sock, text, delay):
    data = text.encode("ascii")
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
    parser = argparse.ArgumentParser(description="Run configure-style srvros CLI probes in QEMU.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--shell-wait", type=float, default=2)
    parser.add_argument("--line-wait", type=float, default=4)
    parser.add_argument("--after-wait", type=float, default=1)
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

    script = (
        "mkdir --parents /fat/cfg/src /fat/cfg/build /fat/cfg/install/bin\n"
        "write /fat/cfg/src/input.txt alpha\n"
        "write -a /fat/cfg/src/input.txt beta\n"
        "install --directory --mode=755 /fat/cfg/prefix/lib /fat/cfg/prefix/include\n"
        "install --mode 644 /fat/cfg/src/input.txt /fat/cfg/prefix/include/input.txt\n"
        "test -f /fat/cfg/prefix/include/input.txt && echo cfg-install-file-ok\n"
        "grep --regexp=alpha --ignore-case /fat/cfg/src/input.txt\n"
        "grep --fixed-strings --quiet beta /fat/cfg/src/input.txt && echo cfg-grep-ok\n"
        "sed --quiet --expression=/beta/p /fat/cfg/src/input.txt\n"
        "head --lines=1 /fat/cfg/src/input.txt\n"
        "tail --lines 1 /fat/cfg/src/input.txt\n"
        "wc --lines /fat/cfg/src/input.txt\n"
        "printf 'one two three' | xargs --max-args=2 echo cfg-xargs\n"
        "printf 'tee-one\\n' | tee --append /fat/cfg/tee.txt\n"
        "printf 'tee-two\\n' | tee --append /fat/cfg/tee.txt\n"
        "tail --lines=2 /fat/cfg/tee.txt\n"
        "find /fat/cfg -name input.txt -type f -print\n"
        "cp --recursive /fat/cfg/src /fat/cfg/copy\n"
        "cat /fat/cfg/copy/input.txt\n"
        "tar --create --file=/fat/cfg/archive.tar /fat/cfg/copy\n"
        "tar --list --file=/fat/cfg/archive.tar\n"
        "mkdir --parents /fat/cfg/extract\n"
        "tar --extract --file=/fat/cfg/archive.tar --directory /fat/cfg/extract\n"
        "cat /fat/cfg/extract/fat/cfg/copy/input.txt\n"
        "write /fat/cfg/Makefile 'OUT = /fat/cfg/build/out.txt'\n"
        "write -a /fat/cfg/Makefile 'all: $(OUT)'\n"
        "write -a /fat/cfg/Makefile '$(OUT): /fat/cfg/src/input.txt'\n"
        "write -a /fat/cfg/Makefile 'cp $< $@'\n"
        "make --file=/fat/cfg/Makefile --dry-run all\n"
        "make --always-make --file /fat/cfg/Makefile all\n"
        "cat /fat/cfg/build/out.txt\n"
        "ln --symbolic /fat/cfg/src/input.txt /fat/cfg/input.link || echo cfg-ln-unsupported-ok\n"
        "rm --recursive --force /fat/cfg\n"
        "test ! -e /fat/cfg && echo cfg-clean-ok\n"
        "exit\n"
    )

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-configure-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-configure.exfat")
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
            output += read_until(sock, b" $ ", args.shell_wait)
            for line in script.splitlines(True):
                send_serial(sock, line, args.send_delay)
                if line.strip() == "exit":
                    output += read_until(sock, b"srv> ", max(args.line_wait, 8.0))
                else:
                    output += read_until(sock, b" $ ", args.line_wait)
            send_serial(sock, "fsck /fat\n", args.send_delay)
            output += read_until(sock, b"srv> ", 15)
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
        "cfg-install-file-ok",
        "alpha",
        "cfg-grep-ok",
        "beta",
        "2 /fat/cfg/src/input.txt",
        "cfg-xargs one two",
        "cfg-xargs three",
        "tee-one",
        "tee-two",
        "/fat/cfg/src/input.txt",
        "/fat/cfg/prefix/include/input.txt",
        "fat/cfg/copy/input.txt",
        "cp /fat/cfg/src/input.txt /fat/cfg/build/out.txt",
        "cfg-ln-unsupported-ok",
        "cfg-clean-ok",
        "exfat-check:",
        "errors=0 ok",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("configure-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("configure-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3
    print("configure-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
