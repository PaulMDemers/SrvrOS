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


def run_monitor_command(sock, command, expect, timeout):
    sock.sendall((command + "\n").encode("utf-8"))
    data = read_until(sock, expect.encode("utf-8"), timeout)
    if expect.encode("utf-8") not in data:
        raise RuntimeError(f"missing expected output {expect!r}")
    data += read_until(sock, b"srv> ", timeout)
    return data


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
        if not text.endswith("\n"):
            handle.write("\n")


def main():
    parser = argparse.ArgumentParser(description="Smoke-test srvros public node:sqlite shim persistence.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--node-elf", default="build/node-srvros-runtime/node-srvros-stripped.elf")
    parser.add_argument("--memory", default="768M")
    parser.add_argument("--boot-wait", type=float, default=90)
    parser.add_argument("--runtime-wait", type=float, default=70)
    parser.add_argument("--skip-build", action="store_true")
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

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")
    qemu = shutil.which(args.qemu, path=env.get("PATH")) or args.qemu

    serial_port = random.randint(24000, 29000)
    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-node-sqlite-shim-") as temp_dir:
        write_script = os.path.join(temp_dir, "sqlite-write.js")
        read_script = os.path.join(temp_dir, "sqlite-read.js")
        disk = os.path.join(temp_dir, "srvros-node.exfat")
        write_text(write_script, """
const { DatabaseSync } = require('node:sqlite');
const db = new DatabaseSync('/fat/sqlite-shim-persist.db');
db.exec('CREATE TABLE IF NOT EXISTS users(id INTEGER, name TEXT, created_at TEXT); DELETE FROM users;');
let first = db.prepare('INSERT INTO users(name, created_at) VALUES (:name, :created_at)').run({ name: 'Ada', created_at: '1' });
db.prepare('INSERT INTO users(name, created_at) VALUES (?, ?)').run('Grace', '2');
let row = db.prepare('SELECT name FROM users WHERE id = ?').get(first.lastInsertRowid);
let count = db.prepare('SELECT COUNT(*) AS count FROM users').get().count;
console.log('SQLITE-WRITE', row.name, count);
db.close();
""")
        write_text(read_script, """
const { DatabaseSync } = require('node:sqlite');
const db = new DatabaseSync('/fat/sqlite-shim-persist.db');
let rows = db.prepare('SELECT id, name FROM users ORDER BY id DESC').all();
let count = db.prepare('SELECT COUNT(*) AS count FROM users WHERE name = :name').get({ name: 'Ada' }).count;
let gone = db.prepare('DELETE FROM users WHERE name = ?').run('Grace').changes;
console.log('SQLITE-READ', rows[0].name, rows[1].name, count, gone);
db.close();
""")
        subprocess.check_call([
            sys.executable,
            os.path.join(root, "tools", "mk_exfat_image.py"),
            disk,
            f"node={node_elf}",
            f"sqlite-write.js={write_script}",
            f"sqlite-read.js={read_script}",
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
            "-monitor", "none",
            "-display", "none",
            "-no-reboot",
        ]
        startupinfo, creationflags = hidden_startup_flags()
        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
            startupinfo=startupinfo, creationflags=creationflags)
        try:
            sock = connect_serial(serial_port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            if b"srv> " not in output:
                raise RuntimeError("monitor prompt not observed")
            output += run_monitor_command(sock, "run /fat/bin/node /fat/bin/sqlite-write.js",
                "SQLITE-WRITE Ada 2", args.runtime_wait)
            output += run_monitor_command(sock, "run /fat/bin/node /fat/bin/sqlite-read.js",
                "SQLITE-READ Grace Ada 1 1", args.runtime_wait)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    sys.stdout.write(output.decode("utf-8", "replace"))
    print("node-sqlite-shim-smoke: ok persistence")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
