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
            chunk = sock.recv(8192)
            if not chunk:
                break
            chunks.append(chunk)
        except socket.timeout:
            pass
    return b"".join(chunks)


def read_until_any(sock, markers, seconds):
    data = b""
    deadline = time.time() + seconds
    while not any(marker in data for marker in markers) and time.time() < deadline:
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


def default_ovmf_path():
    env = os.environ.get("OVMF_CODE")
    if env:
        return env
    candidates = [
        r"C:\msys64\ucrt64\share\qemu\edk2-x86_64-code.fd",
        "/ucrt64/share/qemu/edk2-x86_64-code.fd",
    ]
    for candidate in candidates:
        if os.path.exists(candidate):
            return candidate
    return candidates[0]


def main():
    parser = argparse.ArgumentParser(description="Boot the srvros GPT/FAT32 USB image through OVMF.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--image", default="build/srvros-usb.img")
    parser.add_argument("--ovmf", default=default_ovmf_path())
    parser.add_argument("--boot-wait", type=float, default=90)
    parser.add_argument("--memory", default="2G")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    source_image = args.image if os.path.isabs(args.image) else os.path.join(root, args.image)
    serial_port = random.randint(30000, 39000)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-uefi-usb-") as temp_dir:
        image = os.path.join(temp_dir, "srvros-usb.img")
        shutil.copyfile(source_image, image)
        command = [
            args.qemu,
            "-M", "q35",
            "-m", args.memory,
            "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf}",
            "-drive", f"if=none,id=usb,file={image},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=usb,bus=ahci.0",
            "-serial", f"tcp:127.0.0.1:{serial_port},server,nowait",
            "-monitor", "none",
            "-display", "none",
            "-no-reboot",
        ]
        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            sock = connect_serial(serial_port, 20)
            sock.settimeout(0.3)
            output += read_until_any(sock, [b"srv> ", b" $ "], args.boot_wait)
            if b"srv> " in output:
                sock.sendall(b"bootinfo\n")
                output += read_until_any(sock, [b"srv> "], 10)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)

    missing = []
    if "exfat: mounted /fat" not in text:
        missing.append("exfat mount")
    if "srv>" not in text and " $ " not in text:
        missing.append("shell or monitor prompt")
    if "pci: devices=" not in text:
        missing.append("PCI inventory")
    if "config=ecam" not in text:
        missing.append("ECAM PCI config")
    if has_fatal_exception(text):
        print("uefi-usb-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("uefi-usb-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("uefi-usb-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
