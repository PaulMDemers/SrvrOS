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


def read_until_marker_line(sock, marker, seconds):
    data = b""
    deadline = time.time() + seconds
    marker_text = marker.decode("ascii")
    while time.time() < deadline:
        text = data.decode("utf-8", "replace")
        if any(line.strip() == marker_text for line in text.splitlines()):
            break
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


def send_serial_line(sock, line, byte_delay=0.004):
    for byte in line.encode("ascii"):
        sock.sendall(bytes([byte]))
        time.sleep(byte_delay)


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def has_smoke_failure(text):
    failure_fragments = [
        "sh: unmatched quote",
        "differ",
    ]
    return any(fragment in text for fragment in failure_fragments)


def main():
    parser = argparse.ArgumentParser(description="Verify srvros staged ports in QEMU.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--line-wait", type=float, default=60)
    parser.add_argument("--key-delay", type=float, default=0.004)
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
            send_serial_line(sock, "run /fat/bin/sh\n", args.key_delay)
            output += read_until(sock, b" $ ", 5)
            lines = [
                "zlibdemo\n",
                "jsondemo\n",
                "inidemo\n",
                "linedemo\n",
                "sqlitedemo\n",
                "uvdemo\n",
                "nodeprobe\n",
                "ttydemo\n",
                "posixdemo\n",
                "sh /fat/share/examples/ports-smoke-ed.sh\n",
                "sh /fat/share/examples/ports-smoke-miniport.sh\n",
                "mkdir -p /fat/byacctest\n",
                "cd /fat/byacctest\n",
                "write grammar.y '%token WORD'\n",
                "write -a grammar.y '%%'\n",
                "write -a grammar.y 'line: WORD ;'\n",
                "write -a grammar.y '%%'\n",
                "stat grammar.y\n",
                "byacc -d grammar.y\n",
                "stat y.tab.c\n",
                "stat y.tab.h\n",
                "grep yyparse y.tab.c\n",
                "cd /\n",
                "mkdir -p /fat/ziptest/out\n",
                "write /fat/ziptest/alpha.txt alpha-zip\n",
                "write /fat/ziptest/beta.txt beta-zip\n",
                "cd /fat/ziptest\n",
                "minizip archive.zip alpha.txt beta.txt\n",
                "miniunz -l archive.zip\n",
                "miniunz archive.zip -d out\n",
                "cat out/alpha.txt\n",
                "cat out/beta.txt\n",
                "cd /\n",
                "exit\n",
            ]
            for index, line in enumerate(lines):
                if line.strip() == "exit":
                    send_serial_line(sock, line, args.key_delay)
                    output += read_until(sock, b"srv> ", args.line_wait)
                    continue
                marker = f"__SMOKE_STEP_{index:03d}__"
                wrapped = line.rstrip("\n") + f"; echo {marker}\n"
                send_serial_line(sock, wrapped, args.key_delay)
                marker_bytes = marker.encode("ascii")
                chunk = read_until_marker_line(sock, marker_bytes, args.line_wait)
                output += chunk
                chunk_text = chunk.decode("utf-8", "replace")
                if not any(line.strip() == marker for line in chunk_text.splitlines()):
                    raise RuntimeError(f"timed out waiting for command marker {marker}")
                output += read_for(sock, 0.2)
            send_serial_line(sock, "fsck /fat\n", args.key_delay)
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
        "sqlitedemo: query ok",
        "sqlitedemo: db size=",
        "sqlitedemo: ok sqlite 3.53.1",
        "uvdemo: timer ok",
        "uvdemo: fs ok",
        "uvdemo: async ok",
        "uvdemo: work ok",
        "uvdemo: poll ok",
        "uvdemo: basic ok",
        "nodeprobe: time/random ok",
        "nodeprobe: mmap ok",
        "nodeprobe: fs/fd ok",
        "nodeprobe: libc helpers ok",
        "nodeprobe: pthread ok",
        "nodeprobe: resource ok",
        "nodeprobe: socket/dns ok",
        "nodeprobe: uv/diagnostic stubs ok",
        "nodeprobe: ok",
        "ttydemo: raw mode ok",
        "ttydemo: restore ok",
        "ttydemo: winsize ok",
        "ttydemo: winsize set ok",
        "ttydemo: dup tty ok",
        "ttydemo: enotty ok",
        "ttydemo: ok",
        "posixdemo: fstat-size=",
        "posixdemo: dup ok",
        "posixdemo: fd capacity ok",
        "posixdemo: stdio ok",
        "posixdemo: rw ok",
        "posixdemo: dup write ok",
        "posixdemo: path scan ok",
        "posixdemo: fs api ok",
        "lockprobe: conflict ok",
        "posixdemo: file lock ok",
        "posixdemo: nonblock ok",
        "posixdemo: poll ok",
        "posixdemo: pipe ok",
        "posixdemo: sbrk ok",
        "posixdemo: stdlib extra ok",
        "posixdemo: scanf ok",
        "posixdemo: math ok",
        "posixdemo: regex ok",
        "posixdemo: pread ok",
        "posixdemo: posix misc ok",
        "posixdemo: spawn attrs ok",
        "posixdemo: spawn many actions ok",
        "posixdemo: spawn ok",
        "posixdemo: execve ok",
        "posixdemo: cloexec ok",
        "posixdemo: pthread compat ok",
        "posixdemo: ok",
        "ed-smoke-ok",
        "patch: src/miniport.sh",
        "cp src/miniport.sh build/miniport",
        "install -D build/miniport /fat/local/bin/miniport",
        "miniport-patched",
        "rm -r build",
        "stat: not found: build/miniport",
        "grammar.y:",
        "y.tab.c:",
        "y.tab.h:",
        "yyparse",
        "archive.zip",
        "alpha.txt",
        "beta.txt",
        "alpha-zip",
        "beta-zip",
        "exfat-check:",
        "errors=0 ok",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("ports-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if has_smoke_failure(text):
        print("ports-smoke: command failure detected", file=sys.stderr)
        return 4
    if missing:
        print("ports-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("ports-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
