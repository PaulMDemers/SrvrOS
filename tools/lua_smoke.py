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
    parser = argparse.ArgumentParser(description="Verify srvros Lua port in QEMU.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--line-wait", type=float, default=6)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    port = random.randint(29001, 34000)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-lua-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-lua.exfat")
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
            commands = [
                "lua -e \"print('lua-e-ok', 21+21)\"\n",
                "lua -e \"print('lua-float-ok', 3/2)\"\n",
                "lua -e \"print('lua-type-ok', math.type(3/2))\"\n",
                "lua -e \"print('lua-sqrt-ok', math.sqrt(81))\"\n",
                "lua -e \"print('lua-math-ok', math.sin(0), math.log(100, 10), math.pow and 'compat' or 'stock')\"\n",
                "write /fat/lua-stdin.lua \"print('lua-stdin-ok', 6*7)\"\n",
                "cat /fat/lua-stdin.lua | lua\n",
                "write /fat/lua-test.lua \"print('lua-file-ok')\"\n",
                "lua /fat/lua-test.lua\n",
                "write /fat/mymod.lua \"local M = {}; M.value = 123; return M\"\n",
                "lua -e \"local m=require('mymod'); print('require-ok', m.value)\"\n",
                "lua -e \"local f=io.open('/fat/lio','w'); f:write('abc'); f:close()\"\n",
                "lua -e \"local r=io.open('/fat/lio','r'); print('io-ok', r:read('*a'))\"\n",
                "exit\n",
            ]
            for command_line in commands:
                sock.sendall(command_line.encode("ascii"))
                output += read_until(sock, b"srv> " if command_line.strip() == "exit" else b" $ ", args.line_wait)
            sock.sendall(b"fsck /fat\n")
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
        "lua-e-ok",
        "lua-float-ok",
        "1.5",
        "lua-type-ok",
        "float",
        "lua-sqrt-ok",
        "9",
        "lua-math-ok",
        "lua-stdin-ok",
        "lua-file-ok",
        "require-ok",
        "123",
        "io-ok",
        "abc",
        "exfat-check:",
        "errors=0 ok",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("lua-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("lua-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("lua-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
