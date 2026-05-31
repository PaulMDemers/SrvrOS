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


ROUTES = [
    ("/", b"ring-3 web server"),
    ("/hello.html", b"Hello from srvros"),
    ("/status.txt", b"static file serving from /fat/www is online"),
]


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
        except (ConnectionResetError, OSError):
            break
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


def http_get(port, path, timeout):
    request = f"GET {path} HTTP/1.0\r\nHost: localhost\r\n\r\n".encode("ascii")
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        sock.sendall(request)
        return read_for(sock, timeout)


def hidden_startup_flags():
    if os.name != "nt":
        return None, 0
    startupinfo = subprocess.STARTUPINFO()
    startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startupinfo.wShowWindow = 0
    return startupinfo, getattr(subprocess, "CREATE_NO_WINDOW", 0)


def main():
    parser = argparse.ArgumentParser(
        description="Smoke-test the srvros Node static HTTP demo.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/node-srvros-runtime/srvros-node.exfat")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--boot-wait", type=float, default=90)
    parser.add_argument("--listen-wait", type=float, default=60)
    parser.add_argument("--http-wait", type=float, default=15)
    parser.add_argument("--rounds", type=int, default=2)
    parser.add_argument("--request-retries", type=int, default=2,
        help="Retry an individual route this many times after an empty or incomplete response.")
    parser.add_argument("--memory", default="768M")
    parser.add_argument("--display", default="none",
        help="QEMU display backend. Defaults to none so smoke runs do not open a QEMU window.")
    parser.add_argument("--jitless", action=argparse.BooleanOptionalAction, default=True,
        help="Run the demo under Node --jitless. Enabled by default while srvros V8 compiler tiers mature.")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    if not args.skip_build:
        subprocess.check_call(["make", "node-runtime-image"], cwd=root)
    if not os.path.exists(iso):
        raise FileNotFoundError(iso)
    if not os.path.exists(source_disk):
        raise FileNotFoundError(source_disk)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")
    qemu = shutil.which(args.qemu, path=env.get("PATH")) or args.qemu

    serial_port = random.randint(24000, 29000)
    http_port = random.randint(18100, 18900)
    output = b""
    retry_count = 0
    failures = []
    startup_error = None

    with tempfile.TemporaryDirectory(prefix="srvros-node-http-demo-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-node.exfat")
        shutil.copyfile(source_disk, disk)
        command = [
            qemu,
            "-M", "q35",
            "-m", args.memory,
            "-cdrom", iso,
            "-boot", "d",
            "-serial", f"tcp:127.0.0.1:{serial_port},server,nowait",
            "-drive", f"if=none,id=exfat,file={disk},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=exfat,bus=ahci.0",
            "-netdev", f"user,id=net0,hostfwd=tcp:127.0.0.1:{http_port}-10.0.2.15:8080",
            "-device", "e1000,netdev=net0",
            "-monitor", "none",
            "-display", args.display,
            "-no-reboot",
        ]
        startupinfo, creationflags = hidden_startup_flags()
        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
            startupinfo=startupinfo, creationflags=creationflags)
        try:
            print(
                f"node-http-demo-smoke: booting qemu serial={serial_port} http={http_port}",
                flush=True)
            sock = connect_serial(serial_port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            if b"srv> " not in output:
                startup_error = "monitor prompt not observed"
            else:
                node_args = " --jitless" if args.jitless else ""
                sock.sendall(
                    f"run /fat/bin/node{node_args} /fat/bin/node-http-demo.js\n".encode("ascii"))
                output += read_until(sock, b"NODEHTTP-LISTEN 8080", args.listen_wait)
                if b"NODEHTTP-LISTEN 8080" not in output:
                    startup_error = "Node HTTP demo did not start listening"
                else:
                    print("node-http-demo-smoke: server listening", flush=True)

                    for round_index in range(args.rounds):
                        for path, expected in ROUTES:
                            response = b""
                            attempts = args.request_retries + 1
                            for attempt in range(1, attempts + 1):
                                print(
                                    f"node-http-demo-smoke: GET {path} "
                                    f"round={round_index + 1}/{args.rounds} "
                                    f"attempt={attempt}/{attempts}",
                                    flush=True)
                                response = http_get(http_port, path, args.http_wait)
                                if b"HTTP/1.1 200 OK" in response and expected in response:
                                    break
                                if attempt < attempts:
                                    retry_count += 1
                                    time.sleep(0.2)
                            else:
                                failures.append(
                                    f"round {round_index + 1} {path} incomplete response: {response!r}")
                            output += read_for(sock, 0.5)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)
    if startup_error is not None:
        print(f"node-http-demo-smoke: {startup_error}", file=sys.stderr)
        return 4
    if failures:
        print("node-http-demo-smoke: failures:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    if "exception:" in text:
        print("node-http-demo-smoke: fatal exception detected", file=sys.stderr)
        return 2
    print(
        f"node-http-demo-smoke: ok rounds={args.rounds} "
        f"requests={args.rounds * len(ROUTES)} retries={retry_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
