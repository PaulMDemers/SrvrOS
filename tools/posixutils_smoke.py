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
    parser = argparse.ArgumentParser(description="Run srvros POSIX utility pack smoke coverage.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--line-wait", type=float, default=1.5)
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

    script = (
        "echo posixutils-start\n"
        "ln --help\n"
        "sync --help\n"
        "/fat/bin/test --help\n"
        "/fat/bin/[ --help\n"
        "cksum --help\n"
        "sum --help\n"
        "comm --help\n"
        "paste --help\n"
        "join --help\n"
        "split --help\n"
        "od --help\n"
        "hexdump --help\n"
        "strings --help\n"
        "file --help\n"
        "tty --help\n"
        "stty --help\n"
        "time --help\n"
        "timeout --help\n"
        "nohup --help\n"
        "nice --help\n"
        "write /fat/pu-a.txt apple\n"
        "write -a /fat/pu-a.txt banana\n"
        "write -a /fat/pu-a.txt carrot\n"
        "write /fat/pu-b.txt apple\n"
        "write -a /fat/pu-b.txt berry\n"
        "write -a /fat/pu-b.txt carrot\n"
        "/fat/bin/test -f /fat/pu-a.txt && echo external-test-ok\n"
        "/fat/bin/[ 7 -gt 3 ] && echo bracket-result:ok\n"
        "ln /fat/pu-a.txt /fat/pu-link.txt || echo ln-unsupported-ok\n"
        "sync && echo sync-ok\n"
        "cksum /fat/pu-a.txt\n"
        "sum /fat/pu-a.txt\n"
        "comm /fat/pu-a.txt /fat/pu-b.txt\n"
        "paste /fat/pu-a.txt /fat/pu-b.txt\n"
        "write /fat/join-a.txt '1 alpha'\n"
        "write -a /fat/join-a.txt '2 beta'\n"
        "write /fat/join-b.txt '1 one'\n"
        "write -a /fat/join-b.txt '2 two'\n"
        "join /fat/join-a.txt /fat/join-b.txt\n"
        "split -l 2 /fat/pu-a.txt /fat/split.\n"
        "cat /fat/split.aa\n"
        "cat /fat/split.ab\n"
        "od -tx1 /fat/pu-a.txt | head -n 1\n"
        "hexdump -C /fat/pu-a.txt | head -n 1\n"
        "strings -n 5 /fat/bin/sh | head -n 1\n"
        "file /fat/bin/sh /fat/pu-a.txt /fat\n"
        "tty\n"
        "stty -a\n"
        "time true\n"
        "timeout 1 sleep 2 || echo timeout-killed-ok\n"
        "nohup echo nohup-ok\n"
        "nice -n 5 echo nice-ok\n"
        "echo posixutils-done\n"
        "exit\n"
    )

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-posixutils-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-posixutils.exfat")
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
            for line in script.splitlines(True):
                sock.sendall(line.encode("ascii"))
                output += read_until(sock, b" $ " if line.strip() != "exit" else b"srv> ", args.line_wait)
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
        "usage: ln [-s] source target",
        "usage: sync",
        "usage: test expression",
        "usage: [ expression ]",
        "usage: cksum [file ...]",
        "usage: sum [file ...]",
        "usage: comm [-123] file1 file2",
        "usage: paste [-d delimiters] file ...",
        "usage: join file1 file2",
        "usage: split [-l lines|-b bytes] [file [prefix]]",
        "usage: od [-An] [-tx1] [file ...]",
        "usage: hexdump [-C] [file ...]",
        "usage: strings [-n length] [file ...]",
        "usage: file path ...",
        "usage: tty [-s]",
        "usage: stty [-a|raw|cooked|sane|echo|-echo]",
        "usage: time command [arg ...]",
        "usage: timeout seconds command [arg ...]",
        "usage: nohup command [arg ...]",
        "usage: nice [-n adjustment] command [arg ...]",
        "/fat/pu-a.txt",
        "berry",
        "apple\tapple",
        "1 alpha one",
        "2 beta two",
        "banana",
        "carrot",
        "00000000",
        "ELF 64-bit executable",
        "ASCII text",
        "/fat: directory",
        "/dev/console",
        "icanon",
        "real ",
    ]
    line_expected = [
        "posixutils-start",
        "external-test-ok",
        "bracket-result:ok",
        "ln-unsupported-ok",
        "sync-ok",
        "timeout-killed-ok",
        "nohup-ok",
        "nice-ok",
        "posixutils-done",
    ]
    lines = {line.strip() for line in text.splitlines()}
    missing = [marker for marker in expected if marker not in text]
    missing += [marker for marker in line_expected if marker not in lines]
    if has_fatal_exception(text):
        print("posixutils-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("posixutils-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3
    print("posixutils-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
