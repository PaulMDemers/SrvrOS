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


def http_request(port, method, path, timeout):
    deadline = time.time() + timeout
    request = f"{method} {path} HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n".encode("ascii")
    last_error = None
    last_response = b""
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=2) as sock:
                sock.settimeout(2)
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
                last_response = b"".join(chunks)
                if b"HTTP/1.1" in last_response:
                    return last_response
        except OSError as exc:
            last_error = exc
        time.sleep(0.5)
    if last_response:
        return last_response
    raise RuntimeError(f"http request failed: {last_error}")


def http_get(port, path, timeout):
    return http_request(port, "GET", path, timeout)


def http_head(port, path, timeout):
    return http_request(port, "HEAD", path, timeout)


def http_get_while_slow_peer_open(port, path, timeout):
    slow_sock = socket.create_connection(("127.0.0.1", port), timeout=2)
    slow_sock.settimeout(2)
    try:
        slow_sock.sendall(b"GET / HTTP/1.1\r\nHost: slow-peer\r\n")
        time.sleep(0.5)
        return http_get(port, path, timeout)
    finally:
        try:
            slow_sock.close()
        except OSError:
            pass


def closed_port_refused(port, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=1) as sock:
                sock.settimeout(1)
                sock.sendall(b"x")
                return sock.recv(1) == b""
        except (ConnectionRefusedError, ConnectionResetError, OSError):
            return True
    return False


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def body_bytes(response):
    marker = b"\r\n\r\n"
    index = response.find(marker)
    return b"" if index < 0 else response[index + len(marker):]


def main():
    parser = argparse.ArgumentParser(description="Boot srvros with e1000 and verify webd over host forwarding.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--shell-wait", type=float, default=2)
    parser.add_argument("--service-wait", type=float, default=8)
    parser.add_argument("--settle-wait", type=float, default=1)
    parser.add_argument("--http-wait", type=float, default=25)
    parser.add_argument("--memory", default="512M")
    parser.add_argument("--hostfwd-target", default="10.0.2.15:80")
    parser.add_argument("--pcap", default="")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    serial_port = random.randint(24000, 29000)
    http_port = random.randint(18080, 18999)
    closed_port = random.randint(19000, 19999)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    response = b""
    css_response = b""
    head_response = b""
    slow_peer_response = b""
    large_response = b""
    missing_response = b""
    post_response = b""
    closed_refused = False
    http_error = None
    css_error = None
    head_error = None
    slow_peer_error = None
    large_error = None
    missing_error = None
    post_error = None
    with tempfile.TemporaryDirectory(prefix="srvros-web-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-web.exfat")
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
            "-netdev", f"user,id=net0,hostfwd=tcp:127.0.0.1:{http_port}-{args.hostfwd_target},hostfwd=tcp:127.0.0.1:{closed_port}-10.0.2.15:81",
            "-device", "e1000,netdev=net0",
            "-monitor", "none",
            "-no-reboot",
        ]
        if args.pcap:
            pcap = args.pcap if os.path.isabs(args.pcap) else os.path.join(root, args.pcap)
            command.extend(["-object", f"filter-dump,id=webdump,netdev=net0,file={pcap}"])

        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            sock = connect_serial(serial_port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            sock.sendall(b"run /fat/bin/sh\n")
            output += read_until(sock, b"srvsh: interactive shell", args.shell_wait)
            output += poll_command(sock, "service webd status", "webd background pid", args.service_wait)
            output += send_command(sock, "service list", "enabled=true", args.service_wait)
            output += send_command(sock, "ps", "svscan", args.service_wait)
            output += send_command(sock, "cat /fat/var/log/svscan.log", "svscan: webd started pid", args.service_wait)
            output += send_command(sock, "service webd stop", "stopped pid", args.service_wait)
            output += poll_command(sock, "service webd status", "webd background pid", args.service_wait)
            output += send_command(sock, "cat /fat/var/log/svscan.log", "svscan: webd restarting", args.service_wait)
            output += send_command(sock, "netstat", "LISTEN", args.service_wait)
            output += send_command(sock, "ifconfig", "rx frames", args.service_wait)
            output += read_for(sock, args.settle_wait)
            try:
                response = http_get(http_port, "/", args.http_wait)
            except RuntimeError as exc:
                http_error = exc
            try:
                css_response = http_get(http_port, "/assets/site.css", args.http_wait)
            except RuntimeError as exc:
                css_error = exc
            try:
                head_response = http_head(http_port, "/hello.html", args.http_wait)
            except RuntimeError as exc:
                head_error = exc
            try:
                slow_peer_response = http_get_while_slow_peer_open(http_port, "/status.txt", args.http_wait)
            except (OSError, RuntimeError) as exc:
                slow_peer_error = exc
            try:
                large_response = http_get(http_port, "/large.txt", args.http_wait)
            except RuntimeError as exc:
                large_error = exc
            try:
                missing_response = http_get(http_port, "/missing.txt", args.http_wait)
            except RuntimeError as exc:
                missing_error = exc
            try:
                post_response = http_request(http_port, "POST", "/", args.http_wait)
            except RuntimeError as exc:
                post_error = exc
            closed_refused = closed_port_refused(closed_port, 3)
            sock.sendall(b"service webd tail 8\n")
            output += read_until(sock, b"webd: access POST / 405", args.service_wait)
            sock.sendall(b"service webd log\n")
            output += read_until(sock, b"webd: serving", args.service_wait)
            sock.sendall(b"cat /fat/var/log/svscan.log\n")
            output += read_until(sock, b"svscan: webd restarting", args.service_wait)
            output += read_for(sock, 3)
            if (http_error is not None or
                    css_error is not None or
                    head_error is not None or
                    slow_peer_error is not None or
                    large_error is not None or
                    missing_error is not None or
                    post_error is not None or
                    not closed_refused or
                    b"HTTP/1.1" not in response or
                    b"HTTP/1.1" not in css_response or
                    b"HTTP/1.1" not in head_response or
                    b"HTTP/1.1" not in slow_peer_response or
                    b"HTTP/1.1" not in large_response or
                    b"HTTP/1.1" not in missing_response or
                    b"HTTP/1.1" not in post_response):
                sock.sendall(b"exit\n")
                output += read_until(sock, b"srv> ", 5)
                sock.sendall(b"net\n")
                output += read_until(sock, b"srv> ", 5)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)
    sys.stdout.write(response.decode("utf-8", "replace"))
    sys.stdout.write(css_response.decode("utf-8", "replace"))
    sys.stdout.write(head_response.decode("utf-8", "replace"))
    sys.stdout.write(slow_peer_response.decode("utf-8", "replace"))
    sys.stdout.write(large_response.decode("utf-8", "replace"))
    sys.stdout.write(missing_response.decode("utf-8", "replace"))
    sys.stdout.write(post_response.decode("utf-8", "replace"))

    expected_serial = [
        "e1000:",
        "net: static ipv4=10.0.2.15",
        "init: started pid=",
        "init: system init starting",
        "init-script-ok",
        "svscan",
        "svscan: started",
        "svscan: webd started pid",
        "webd: serving /fat/www on 10.0.2.15:80",
        "webd background pid",
        "enabled=true",
        "restart=always",
        "stopped pid",
        "svscan: webd restarting",
        "log /fat/var/log/webd.log",
        "webd: access GET / 200",
        "webd: access GET /missing.txt 404",
        "webd: access POST / 405",
        "Proto State",
        "10.0.2.15:80",
        "e1000: flags=UP,RUNNING",
        "inet 10.0.2.15",
    ]
    expected_response = [
        b"HTTP/1.1 200 OK",
        b"Content-Length:",
        b"<h1>srvros</h1>",
        b"ring-3 web server",
    ]
    expected_css_response = [
        b"HTTP/1.1 200 OK",
        b"Content-Type: text/css; charset=utf-8",
        b"Content-Length:",
        b"max-width:48rem",
    ]
    expected_head_response = [
        b"HTTP/1.1 200 OK",
        b"Content-Length:",
    ]
    expected_slow_peer_response = [
        b"HTTP/1.1 200 OK",
        b"srvros webd: static file serving from /fat/www is online.",
    ]
    expected_large_response = [
        b"HTTP/1.1 200 OK",
        b"Content-Length: 5982",
        b"srvros large tcp payload begins",
        b"srvros large tcp payload ends",
    ]
    expected_missing_response = [
        b"HTTP/1.1 404 Not Found",
        b"srvros webd: not found",
    ]
    expected_post_response = [
        b"HTTP/1.1 405 Method Not Allowed",
        b"srvros webd: method not allowed",
    ]
    missing = [marker for marker in expected_serial if marker not in text]
    missing += [marker.decode("ascii") for marker in expected_response if marker not in response]
    missing += [marker.decode("ascii") for marker in expected_css_response if marker not in css_response]
    missing += [marker.decode("ascii") for marker in expected_head_response if marker not in head_response]
    missing += [marker.decode("ascii") for marker in expected_slow_peer_response
        if marker not in slow_peer_response]
    missing += [marker.decode("ascii") for marker in expected_large_response
        if marker not in large_response]
    missing += [marker.decode("ascii") for marker in expected_missing_response
        if marker not in missing_response]
    missing += [marker.decode("ascii") for marker in expected_post_response
        if marker not in post_response]
    if body_bytes(head_response):
        missing.append("HEAD response included a body")
    if http_error is not None:
        missing.append(str(http_error))
    if css_error is not None:
        missing.append(str(css_error))
    if head_error is not None:
        missing.append(str(head_error))
    if slow_peer_error is not None:
        missing.append(str(slow_peer_error))
    if large_error is not None:
        missing.append(str(large_error))
    if missing_error is not None:
        missing.append(str(missing_error))
    if post_error is not None:
        missing.append(str(post_error))
    if not closed_refused:
        missing.append("closed TCP port was not refused")
    if has_fatal_exception(text):
        print("web-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("web-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("web-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
