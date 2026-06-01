#!/usr/bin/env python3
import argparse
import base64
import json
import os
import random
import shutil
import socket
import subprocess
import sys
import tempfile
import time


def hidden_startup_flags():
    if os.name != "nt":
        return None, 0
    startupinfo = subprocess.STARTUPINFO()
    startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startupinfo.wShowWindow = 0
    return startupinfo, getattr(subprocess, "CREATE_NO_WINDOW", 0)


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


def http_request(port, method, path, body=None, headers=None, timeout=15):
    headers = dict(headers or {})
    payload = b"" if body is None else json.dumps(body).encode("utf-8")
    if payload:
        headers["Content-Type"] = "application/json"
        headers["Content-Length"] = str(len(payload))
    header_text = "".join(f"{key}: {value}\r\n" for key, value in headers.items())
    request = f"{method} {path} HTTP/1.0\r\nHost: localhost\r\n{header_text}\r\n".encode("ascii") + payload
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        sock.sendall(request)
        response = read_for(sock, timeout)
    head, _, body_bytes = response.partition(b"\r\n\r\n")
    if b"200" not in head.split(b"\r\n", 1)[0]:
        raise RuntimeError(f"unexpected response for {method} {path}: {response!r}")
    return json.loads(body_bytes.decode("utf-8"))


def build_bundle(root):
    app_dir = os.path.join(root, "ports", "node", "express-jwt-sqlite-demo")
    subprocess.check_call(["npm", "run", "build"], cwd=app_dir)
    return os.path.join(app_dir, "dist", "server.bundle.js")


def decode_jwt_header(token):
    head = token.split(".", 1)[0]
    padded = head + ("=" * ((4 - len(head) % 4) % 4))
    return json.loads(base64.urlsafe_b64decode(padded.encode("ascii")).decode("utf-8"))


def main():
    parser = argparse.ArgumentParser(description="Smoke-test the srvros Node Express/JWT/SQLite API demo.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--node-elf", default="build/node-srvros-runtime/node-srvros-stripped.elf")
    parser.add_argument("--memory", default="768M")
    parser.add_argument("--boot-wait", type=float, default=90)
    parser.add_argument("--listen-wait", type=float, default=80)
    parser.add_argument("--http-wait", type=float, default=15)
    parser.add_argument("--rounds", type=int, default=1,
        help="Repeat the health/token/users/secure route set this many times against one guest server.")
    parser.add_argument("--token-path", default="",
        help="Optional GET token route override. Defaults to POST /token with a JSON body.")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--skip-app-build", action="store_true")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    node_elf = args.node_elf if os.path.isabs(args.node_elf) else os.path.join(root, args.node_elf)
    if not args.skip_build:
        subprocess.check_call(["make", "node-runtime-image"], cwd=root)
    if not os.path.exists(iso):
        raise FileNotFoundError(iso)
    if not os.path.exists(node_elf):
        raise FileNotFoundError(node_elf)

    bundle = os.path.join(root, "ports", "node", "express-jwt-sqlite-demo", "dist", "server.bundle.js")
    if not args.skip_app_build or not os.path.exists(bundle):
        bundle = build_bundle(root)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")
    qemu = shutil.which(args.qemu, path=env.get("PATH")) or args.qemu

    serial_port = random.randint(24000, 29000)
    http_port = random.randint(18100, 18900)
    output = b""

    with tempfile.TemporaryDirectory(prefix="srvros-node-express-demo-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-node.exfat")
        subprocess.check_call([
            sys.executable,
            os.path.join(root, "tools", "mk_exfat_image.py"),
            disk,
            f"node={node_elf}",
            f"express-demo.js={bundle}",
        ], cwd=root)

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
            "-display", "none",
            "-no-reboot",
        ]
        startupinfo, creationflags = hidden_startup_flags()
        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
            startupinfo=startupinfo, creationflags=creationflags)
        try:
            print(f"node-express-demo-smoke: booting qemu serial={serial_port} http={http_port}", flush=True)
            sock = connect_serial(serial_port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            if b"srv> " not in output:
                raise RuntimeError("monitor prompt not observed")
            sock.sendall(b"run /fat/bin/node --jitless /fat/bin/express-demo.js\n")
            output += read_until(sock, b"EXPRESS-DEMO-LISTEN 8080", args.listen_wait)
            if b"EXPRESS-DEMO-LISTEN 8080" not in output:
                raise RuntimeError("Express demo did not start listening")

            for index in range(args.rounds):
                subject = f"paul-{index + 1}"
                name = f"Grace-{index + 1}"
                health = http_request(http_port, "GET", "/health", timeout=args.http_wait)
                if health.get("db") != "node:sqlite":
                    raise RuntimeError(f"expected node:sqlite backend, got health {health!r}")
                try:
                    if args.token_path:
                        token = http_request(http_port, "GET", args.token_path, timeout=args.http_wait)["token"]
                        subject = "paul"
                    else:
                        token = http_request(http_port, "POST", "/token",
                            body={"sub": subject}, timeout=args.http_wait)["token"]
                    header = decode_jwt_header(token)
                    if header.get("alg") != "HS256":
                        raise RuntimeError(f"expected jsonwebtoken HS256 token, got header {header!r}")
                    created = http_request(http_port, "POST", "/users",
                        body={"name": name}, timeout=args.http_wait)
                    users = http_request(http_port, "GET", "/users", timeout=args.http_wait)
                    secure = http_request(http_port, "GET", "/secure",
                        headers={"Authorization": f"Bearer {token}"}, timeout=args.http_wait)
                except Exception:
                    output += read_for(sock, 2)
                    raise

                if not health.get("ok") or created["user"]["name"] != name:
                    raise RuntimeError("API payload mismatch")
                if not any(user.get("name") == name for user in users.get("users", [])):
                    raise RuntimeError("created user missing from list")
                if secure.get("claims", {}).get("sub") != subject:
                    raise RuntimeError("JWT validation mismatch")
            output += read_for(sock, 1)
        except Exception:
            sys.stdout.write(output.decode("utf-8", "replace"))
            raise
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)
    if "exception:" in text or "Fatal error" in text:
        print("node-express-demo-smoke: fatal exception detected", file=sys.stderr)
        return 2
    print(f"node-express-demo-smoke: ok health token users secure rounds={args.rounds}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
