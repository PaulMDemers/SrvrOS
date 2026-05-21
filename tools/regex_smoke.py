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


def read_until(sock, marker, timeout):
    deadline = time.time() + timeout
    output = b""
    while time.time() < deadline and marker not in output:
        try:
            chunk = sock.recv(4096)
            if chunk:
                output += chunk
        except socket.timeout:
            pass
    return output


def connect_serial(port, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1)
        except OSError:
            time.sleep(0.1)
    raise RuntimeError("serial connection failed")


def send_serial(sock, text, delay):
    for byte in text.encode("ascii"):
        sock.sendall(bytes([byte]))
        if delay:
            time.sleep(delay)


def main():
    parser = argparse.ArgumentParser(description="Run focused srvros regex/text-tool smoke tests.")
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--memory", default="512M")
    parser.add_argument("--send-delay", type=float, default=0.001)
    parser.add_argument("--boot-wait", type=float, default=60.0)
    parser.add_argument("--line-wait", type=float, default=5.0)
    args = parser.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    iso = os.path.join(root, "build", "srvros-x86_64.iso")
    source_disk = os.path.join(root, "build", "srvros.exfat")
    env = os.environ.copy()
    env["PATH"] = r"C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + env.get("PATH", "")

    subprocess.check_call(["make", "build/srvros-x86_64.iso"], cwd=root, env=env)

    script = (
        "write /fat/re.txt Alpha-123\n"
        "write -a /fat/re.txt beta-45\n"
        "write -a /fat/re.txt gamma\n"
        "grep -E '^(Alpha|beta)-[0-9]{2,3}$' /fat/re.txt\n"
        "grep -o -E '([A-Za-z]+)-([0-9]+)' /fat/re.txt\n"
        "grep -l 'gamma' /fat/re.txt /fat/status.txt\n"
        "grep -L 'gamma' /fat/re.txt /fat/status.txt\n"
        "sed 's/([A-Za-z]+)-([0-9]+)/num=\\\\2 word=\\\\1/' /fat/re.txt\n"
        "write /fat/ed-re-script 'a'\n"
        "write -a /fat/ed-re-script 'foo-12'\n"
        "write -a /fat/ed-re-script 'bar-34'\n"
        "write -a /fat/ed-re-script '.'\n"
        "write -a /fat/ed-re-script '/bar/s/bar-[0-9][0-9]/34:bar/'\n"
        "write -a /fat/ed-re-script '1,2p'\n"
        "write -a /fat/ed-re-script 'q'\n"
        "ed -s < /fat/ed-re-script\n"
        "posixdemo\n"
        "exit\n"
    )

    port = random.randint(24000, 29000)
    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-regex-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-regex.exfat")
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
            output += read_until(sock, b" $ ", args.line_wait)
            for line in script.splitlines(True):
                send_serial(sock, line, args.send_delay)
                output += read_until(sock, b" $ " if line.strip() != "exit" else b"srv> ", args.line_wait)
            send_serial(sock, "fsck /fat\n", args.send_delay)
            output += read_until(sock, b"srv> ", max(args.line_wait, 10.0))
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)
    expected = [
        "Alpha-123",
        "beta-45",
        "/fat/re.txt",
        "/fat/status.txt",
        "num=123 word=Alpha",
        "num=45 word=beta",
        "foo-12",
        "34:bar",
        "posixdemo: regex ok",
        "exfat-check:",
        "errors=0 ok",
    ]
    missing = [marker for marker in expected if marker not in text]
    if missing:
        print("regex-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 1
    print("regex-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
