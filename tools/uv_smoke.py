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


def tcp_request(port, payload, timeout):
    deadline = time.time() + timeout
    last_error = None
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=2) as sock:
                sock.settimeout(2)
                sock.sendall(payload)
                try:
                    sock.shutdown(socket.SHUT_WR)
                except OSError:
                    pass
                chunks = []
                while True:
                    try:
                        chunk = sock.recv(4096)
                    except socket.timeout:
                        break
                    if not chunk:
                        break
                    chunks.append(chunk)
                return b"".join(chunks)
        except OSError as exc:
            last_error = exc
            time.sleep(0.5)
    raise RuntimeError(f"tcp request failed: {last_error}")


def serve_guest_tcp(port, timeout):
    result = {"payload": b"", "error": None}
    ready = threading.Event()

    def worker():
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
                server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                server.bind(("0.0.0.0", port))
                server.listen(1)
                server.settimeout(timeout)
                ready.set()
                conn, _addr = server.accept()
                with conn:
                    conn.settimeout(timeout)
                    chunks = []
                    while True:
                        chunk = conn.recv(4096)
                        if not chunk:
                            break
                        chunks.append(chunk)
                        if b"from-guest" in b"".join(chunks):
                            break
                    result["payload"] = b"".join(chunks)
                    conn.sendall(b"uv-client-ok:" + result["payload"])
        except Exception as exc:
            result["error"] = exc
            ready.set()

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()
    if not ready.wait(timeout=3):
        raise RuntimeError("host tcp server did not start")
    return thread, result


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def main():
    parser = argparse.ArgumentParser(description="Verify the srvros uv.h shim and uvdemo TCP path.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--line-wait", type=float, default=8)
    parser.add_argument("--tcp-wait", type=float, default=25)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    serial_port = random.randint(24000, 29000)
    host_port = random.randint(20000, 23999)
    client_port = random.randint(30000, 33999)
    payloads = [b"from-host-one", b"from-host-two"]
    responses = []
    client_thread = None
    client_result = None

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-uv-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-uv.exfat")
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
            "-netdev", f"user,id=net0,hostfwd=tcp::{host_port}-10.0.2.15:7018",
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
            output += read_until(sock, b" $ ", 6)
            sock.sendall(b"uvdemo\n")
            output += read_until(sock, b"uvdemo: basic ok", args.line_wait)
            output += read_until(sock, b" $ ", 3)
            sock.sendall(b"uvdemo tcp\n")
            output += read_until(sock, b"uvdemo: tcp listening 7018", args.line_wait)
            for payload in payloads:
                responses.append(tcp_request(host_port, payload, args.tcp_wait))
            output += read_until(sock, b" $ ", args.tcp_wait)
            client_thread, client_result = serve_guest_tcp(client_port, args.tcp_wait)
            sock.sendall(f"uvdemo client {client_port}\n".encode("ascii"))
            output += read_until(sock, b"uvdemo: client all ok", args.tcp_wait)
            output += read_until(sock, b" $ ", 3)
            output += read_for(sock, 1)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)
    for response in responses:
        sys.stdout.write(response.decode("utf-8", "replace"))
        sys.stdout.write("\n")
    sys.stdout.write("\n")

    expected = [
        "uvdemo: timer ok",
        "uvdemo: fs ok",
        "uvdemo: async ok",
        "uvdemo: work ok",
        "uvdemo: poll ok",
        "uvdemo: basic ok",
        "uvdemo: tcp listening 7018",
        "uvdemo: tcp accepted",
        "uvdemo: tcp accepted client 1",
        "uvdemo: tcp accepted client 2",
        "uvdemo: tcp read 13",
        "uvdemo: tcp read client 1 13",
        "uvdemo: tcp read client 2 13",
        "uvdemo: tcp ok",
        "uvdemo: tcp ok client 1",
        "uvdemo: tcp shutdown client 1",
        "uvdemo: tcp ok client 2",
        "uvdemo: tcp shutdown client 2",
        "uvdemo: tcp all ok",
        "uvdemo: client connecting",
        "uvdemo: client connected",
        "uvdemo: client write ok",
        "uvdemo: client read 23",
        "uvdemo: client ok",
        "uvdemo: client all ok",
    ]
    missing = [marker for marker in expected if marker not in text]
    expected_responses = [b"uv-tcp-ok:" + payload for payload in payloads]
    if responses != expected_responses:
        missing.append("uv tcp responses")
    if client_thread is not None:
        client_thread.join(timeout=3)
    if client_result is None or client_result.get("payload") != b"from-guest":
        missing.append("uv tcp client payload")
    elif client_result.get("error") is not None:
        missing.append(f"uv tcp client server error: {client_result['error']}")
    if has_fatal_exception(text):
        print("uv-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("uv-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("uv-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
