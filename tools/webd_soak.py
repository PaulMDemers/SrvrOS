#!/usr/bin/env python3
import argparse
import os
import random
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time


REQUEST_CASES = [
    ("/", [b"HTTP/1.1 200 OK", b"<h1>srvros</h1>"]),
    ("/assets/site.css", [b"HTTP/1.1 200 OK", b"Content-Type: text/css", b"max-width:48rem"]),
    ("/status.txt", [b"HTTP/1.1 200 OK", b"static file serving"]),
    ("/large.txt", [b"HTTP/1.1 200 OK", b"Content-Length: 5982", b"srvros large tcp payload ends"]),
    ("/missing.txt", [b"HTTP/1.1 404 Not Found", b"srvros webd: not found"]),
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
    return b"".join(chunks)


def read_until(sock, marker, seconds):
    data = b""
    deadline = time.time() + seconds
    while marker not in data and time.time() < deadline:
        data += read_for(sock, 0.5)
    return data


def send_command(sock, command, marker, timeout):
    sock.sendall(command.encode("ascii") + b"\n")
    return read_until(sock, marker.encode("ascii"), timeout)


def poll_command(sock, command, marker, timeout, interval=0.5):
    data = b""
    deadline = time.time() + timeout
    marker_bytes = marker.encode("ascii")
    while marker_bytes not in data and time.time() < deadline:
        sock.sendall(command.encode("ascii") + b"\n")
        data += read_until(sock, marker_bytes, min(1.0, max(0.1, deadline - time.time())))
        if marker_bytes in data:
            break
        time.sleep(interval)
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
    request = f"GET {path} HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n".encode("ascii")
    deadline = time.time() + timeout
    last_error = None
    last_response = b""
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=2) as sock:
                sock.settimeout(3)
                sock.sendall(request)
                chunks = []
                while True:
                    try:
                        chunk = sock.recv(4096)
                    except socket.timeout:
                        break
                    if not chunk:
                        break
                    chunks.append(chunk)
                response = b"".join(chunks)
                last_response = response
                if b"HTTP/1.1" in response:
                    return response
        except OSError as exc:
            last_error = exc
        time.sleep(0.2)
    if last_response:
        return last_response
    raise RuntimeError(f"GET {path} failed: {last_error}")


def early_disconnect(port, path):
    with socket.create_connection(("127.0.0.1", port), timeout=2) as sock:
        sock.settimeout(1)
        request = f"GET {path} HTTP/1.1\r\nHost: early-close\r\nConnection: close\r\n\r\n".encode("ascii")
        sock.sendall(request)
        try:
            sock.recv(128)
        except socket.timeout:
            pass


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def check_response(label, response, markers, missing):
    for marker in markers:
        if marker not in response:
            missing.append(f"{label} missing {marker.decode('ascii')}")


def concurrent_round(port, clients, timeout, missing):
    lock = threading.Lock()

    def worker(index):
        path, markers = REQUEST_CASES[index % len(REQUEST_CASES)]
        try:
            response = http_get(port, path, timeout)
            local_missing = []
            check_response(f"concurrent {index + 1} {path}", response, markers, local_missing)
            with lock:
                missing.extend(local_missing)
        except Exception as exc:
            with lock:
                missing.append(f"concurrent {index + 1} {path}: {exc}")

    threads = [threading.Thread(target=worker, args=(i,)) for i in range(clients)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join(timeout + 2)
    for index, thread in enumerate(threads):
        if thread.is_alive():
            missing.append(f"concurrent {index + 1} timed out")


def main():
    parser = argparse.ArgumentParser(description="Soak srvros webd with sequential, concurrent, and aborting clients.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--sequential", type=int, default=30)
    parser.add_argument("--concurrent", type=int, default=4)
    parser.add_argument("--boot-wait", type=float, default=25)
    parser.add_argument("--service-wait", type=float, default=12)
    parser.add_argument("--http-wait", type=float, default=25)
    parser.add_argument("--memory", default="512M")
    parser.add_argument("--hostfwd-target", default="10.0.2.15:80")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    serial_port = random.randint(24000, 29000)
    http_port = random.randint(18080, 18999)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    missing = []
    with tempfile.TemporaryDirectory(prefix="srvros-webd-soak-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-webd-soak.exfat")
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
            "-netdev", f"user,id=net0,hostfwd=tcp:127.0.0.1:{http_port}-{args.hostfwd_target}",
            "-device", "e1000,netdev=net0",
            "-monitor", "none",
            "-no-reboot",
        ]
        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            sock = connect_serial(serial_port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            sock.sendall(b"run /fat/bin/sh\n")
            output += read_until(sock, b"srvsh: interactive shell", args.service_wait)
            output += poll_command(sock, "service webd status", "webd background pid", args.service_wait)
            output += send_command(sock, "service webd log", "webd: serving", args.service_wait)
            output += send_command(sock, "netstat", "10.0.2.15:80", args.service_wait)

            for index in range(args.sequential):
                path, markers = REQUEST_CASES[index % len(REQUEST_CASES)]
                try:
                    response = http_get(http_port, path, args.http_wait)
                    check_response(f"sequential {index + 1} {path}", response, markers, missing)
                except Exception as exc:
                    missing.append(f"sequential {index + 1} {path}: {exc}")

            concurrent_round(http_port, args.concurrent, args.http_wait, missing)

            try:
                early_disconnect(http_port, "/large.txt")
            except Exception as exc:
                missing.append(f"early disconnect: {exc}")

            try:
                response = http_get(http_port, "/status.txt", args.http_wait)
                check_response("post-abort /status.txt", response,
                    [b"HTTP/1.1 200 OK", b"static file serving"], missing)
            except Exception as exc:
                missing.append(f"post-abort /status.txt: {exc}")

            output += send_command(sock, "service webd tail 12", "webd: response sent", args.service_wait)
            output += send_command(sock, "netstat", "10.0.2.15:80", args.service_wait)
            output += send_command(sock, "ifconfig", "rx frames", args.service_wait)
            output += read_for(sock, 1)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)

    for marker in [
        "webd background pid",
        "webd: serving /fat/www on 10.0.2.15:80",
        "10.0.2.15:80",
        "e1000: flags=UP,RUNNING",
    ]:
        if marker not in text:
            missing.append(marker)
    if "webd: poll failed" in text:
        missing.append("webd poll failure")
    if has_fatal_exception(text):
        print("webd-soak: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("webd-soak: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print(f"webd-soak: ok sequential={args.sequential} concurrent={args.concurrent}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
