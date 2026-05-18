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


def run_monitor_command(sock, command, marker, timeout, send_delay):
    output = read_for(sock, 0.2)
    send_serial(sock, command + "\n", send_delay)
    output += read_until(sock, marker, timeout)
    output += read_until(sock, b"srv> ", timeout)
    return output


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


def build_script(rounds):
    lines = [
        "echo stress-start\n",
        "ps\n",
    ]
    for i in range(rounds):
        lines.extend([
            f"write /fat/s{i}.txt round-{i}\n",
            f"cat /fat/s{i}.txt\n",
            f"cp /fat/s{i}.txt /fat/c{i}.txt\n",
            f"mv /fat/c{i}.txt /fat/m{i}.txt\n",
            f"cat /fat/m{i}.txt\n",
            f"rm /fat/s{i}.txt\n",
            f"rm /fat/m{i}.txt\n",
            f"stat /fat/s{i}.txt\n",
            "ls /fat/bin > /fat/bl.txt\n",
            "grep sh /fat/bl.txt\n",
            "rm /fat/bl.txt\n",
        ])
    lines.extend([
        "write /fat/long-created-name.txt long-name-ok\n",
        "cat /fat/long-created-name.txt\n",
        "rm /fat/long-created-name.txt\n",
        "cp /fat/bin/sh /fat/shcpy\n",
        "stat /fat/shcpy\n",
        "rm /fat/shcpy\n",
        "echo fragment-start\n",
    ])
    for i in range(5):
        lines.append(f"dd if=/dev/zero of=/fat/frag-fill-{i}.bin bs=4096 count=512 status=none\n")
    lines.append("stat /fat/frag-fill-1.bin\n")
    for i in range(0, 5, 2):
        lines.append(f"rm -f /fat/frag-fill-{i}.bin\n")
    lines.extend([
        "dd if=/dev/zero of=/fat/frag-big.bin bs=4096 count=1024 status=none\n",
        "stat /fat/frag-big.bin\n",
    ])
    for i in range(5):
        lines.append(f"rm -f /fat/frag-fill-{i}.bin\n")
    lines.extend([
        "cp /fat/frag-big.bin /fat/frag-big-copy.bin\n",
        "cmp -s /fat/frag-big.bin /fat/frag-big-copy.bin; echo fragmented-cmp-status-$?\n",
        "rm -f /fat/frag-big.bin /fat/frag-big-copy.bin\n",
    ])
    lines.extend([
        "echo fragment-done\n",
        "echo stress-done\n",
        "exit\n",
    ])
    return "".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Stress srvros filesystem and process churn under QEMU.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--rounds", type=int, default=8)
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--line-wait", type=float, default=2.0)
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
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-fs-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-fs.exfat")
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
            send_serial(sock, "fsck /fat\n", args.send_delay)
            output += read_until(sock, b"srv> ", 10)
            send_serial(sock, "run /fat/bin/sh\n", args.send_delay)
            output += read_until(sock, b" $ ", 5)
            for line in build_script(args.rounds).splitlines(True):
                send_serial(sock, line, args.send_delay)
                if line.strip() == "exit":
                    output += read_until(sock, b"srv> ", 10)
                else:
                    output += read_until(sock, b" $ ", args.line_wait)
            output += run_monitor_command(sock, "fsck /fat", b"exfat-check:", 15, args.send_delay)
            output += run_monitor_command(sock, "memstat", b"pmm:", 10, args.send_delay)
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
        "exfat-check:",
        "errors=0 ok",
        "stress-start",
        "stress-done",
        "long-name-ok",
        "/fat/shcpy:",
        "fragment-start",
        "/fat/frag-fill-1.bin: 2097152 bytes",
        "/fat/frag-big.bin: 4194304 bytes",
        "fragmented-cmp-status-0",
        "fragment-done",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("fs-stress: fatal exception detected", file=sys.stderr)
        return 2
    if "for: too many words" in text:
        print("fs-stress: shell word-limit regression in generated script", file=sys.stderr)
        return 2
    if "command not found:" in text:
        print("fs-stress: generated script command was mangled", file=sys.stderr)
        return 2
    if "dd: close failed" in text:
        print("fs-stress: fragmentation fill ran out of space earlier than expected", file=sys.stderr)
        return 2
    if "dd: invalid" in text or "usage: dd" in text:
        print("fs-stress: generated dd command was malformed", file=sys.stderr)
        return 2
    if "cp: close failed" in text:
        print("fs-stress: fragmented copy ran out of space", file=sys.stderr)
        return 2
    if missing:
        print("fs-stress: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3
    if "errors=0 ok" not in text.split("stress-done")[-1]:
        print("fs-stress: final fsck did not report clean", file=sys.stderr)
        return 4

    print("fs-stress: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
