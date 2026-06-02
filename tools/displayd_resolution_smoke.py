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


def msys_env():
    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")
    return env


def make_limine_conf(width, height):
    return (
        "timeout: 1\n"
        "default_entry: 1\n"
        "serial: yes\n"
        "\n"
        "/srvros\n"
        "    protocol: limine\n"
        f"    resolution: {width}x{height}\n"
        "    path: boot():/boot/srvros.elf\n"
        "    module_path: boot():/boot/initramfs.tar\n"
        "    module_string: initramfs\n"
    )


def build_iso(root, temp_dir, width, height, env):
    source = os.path.join(root, "build", "iso_root")
    iso_root = os.path.join(temp_dir, f"iso_root_{width}x{height}")
    iso = os.path.join(temp_dir, f"srvros-{width}x{height}.iso")
    iso_root_arg = os.path.relpath(iso_root, root).replace(os.sep, "/")
    iso_arg = os.path.relpath(iso, root).replace(os.sep, "/")
    shutil.copytree(source, iso_root)
    conf = make_limine_conf(width, height)
    limine_conf = os.path.join(iso_root, "boot", "limine", "limine.conf")
    with open(limine_conf, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(conf)
    xorriso = shutil.which("xorriso", path=env.get("PATH", ""))
    if xorriso is None:
        raise RuntimeError("xorriso not found; build the normal ISO first from MSYS2/UCRT64")
    subprocess.check_call([
        xorriso,
        "-as", "mkisofs",
        "-R", "-r", "-J",
        "-b", "boot/limine/limine-bios-cd.bin",
        "-no-emul-boot", "-boot-load-size", "4", "-boot-info-table",
        "-hfsplus", "-apm-block-size", "2048",
        "--efi-boot", "boot/limine/limine-uefi-cd.bin",
        "-efi-boot-part", "--efi-boot-image", "--protective-msdos-label",
        iso_root_arg,
        "-o", iso_arg,
    ], cwd=root, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    limine = os.path.join(root, "build", "limine-binary", "limine.exe")
    subprocess.check_call([limine, "bios-install", iso],
        cwd=root, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    return iso


def run_resolution(root, qemu, iso, disk, width, height, args, env):
    port = random.randint(49001, 54000)
    output = b""
    with tempfile.TemporaryDirectory(prefix=f"srvros-displayd-{width}x{height}-disk-") as disk_dir:
        test_disk = os.path.join(disk_dir, "srvros.exfat")
        shutil.copyfile(disk, test_disk)
        command = [
            qemu,
            "-M", "q35",
            "-m", args.memory,
            "-cdrom", iso,
            "-boot", "d",
            "-serial", f"tcp:127.0.0.1:{port},server,nowait",
            "-drive", f"if=none,id=exfat,file={test_disk},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=exfat,bus=ahci.0",
            "-display", "none",
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
            output += read_until(sock, b" $ ", args.shell_wait)
            sock.sendall(b"displayd --smoke\n")
            output += read_until(sock, b"displayd: smoke ok", args.displayd_wait)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)
    expected = [
        f"framebuffer: {width}x{height}",
        f"displayd: framebuffer {width}x{height}",
        "displayd: root backbuffer ready",
        "displayd: smoke ok",
        "displayd: exited",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print(f"displayd-resolution-smoke: fatal exception at {width}x{height}", file=sys.stderr)
        return 2
    if missing:
        print(f"displayd-resolution-smoke: missing markers at {width}x{height}:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3
    print(f"displayd-resolution-smoke: {width}x{height} ok")
    return 0


def parse_resolution(text):
    parts = text.lower().split("x", 1)
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(f"invalid resolution: {text}")
    width = int(parts[0], 10)
    height = int(parts[1], 10)
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError(f"invalid resolution: {text}")
    return width, height


def main():
    parser = argparse.ArgumentParser(description="Run srvros displayd framebuffer-resolution smokes.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--resolution", action="append", type=parse_resolution,
        help="Resolution to test, e.g. 1440x900. May be supplied more than once.")
    parser.add_argument("--boot-wait", type=float, default=45)
    parser.add_argument("--shell-wait", type=float, default=2)
    parser.add_argument("--displayd-wait", type=float, default=25)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    env = msys_env()
    resolutions = args.resolution or [
        (800, 600),
        (1280, 800),
        (1440, 900),
        (1920, 1080),
    ]

    temp_parent = os.path.join(root, "build")
    os.makedirs(temp_parent, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="srvros-displayd-resolutions-", dir=temp_parent) as temp_dir:
        for width, height in resolutions:
            iso = build_iso(root, temp_dir, width, height, env)
            result = run_resolution(root, args.qemu, iso, disk, width, height, args, env)
            if result != 0:
                return result
    print("displayd-resolution-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
