#!/usr/bin/env python3
import argparse
import os
import random
import shutil
import socket
import struct
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


def send_serial(sock, text, delay):
    data = text.encode("ascii")
    if delay <= 0:
        sock.sendall(data)
        return
    for byte in data:
        sock.sendall(bytes([byte]))
        time.sleep(delay)


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


def exfat_geometry(image):
    sector_size = 1 << image[108]
    sectors_per_cluster = 1 << image[109]
    cluster_size = sector_size * sectors_per_cluster
    return {
        "sector_size": sector_size,
        "cluster_size": cluster_size,
        "fat_offset": struct.unpack_from("<I", image, 80)[0] * sector_size,
        "cluster_heap_offset": struct.unpack_from("<I", image, 88)[0] * sector_size,
        "cluster_count": struct.unpack_from("<I", image, 92)[0],
        "root_cluster": struct.unpack_from("<I", image, 96)[0],
    }


def cluster_offset(geo, cluster):
    return geo["cluster_heap_offset"] + (cluster - 2) * geo["cluster_size"]


def bitmap_info(image, geo):
    root = cluster_offset(geo, geo["root_cluster"])
    for offset in range(root, root + geo["cluster_size"], 32):
        entry_type = image[offset]
        if entry_type == 0:
            break
        if entry_type == 0x81:
            cluster = struct.unpack_from("<I", image, offset + 20)[0]
            size = struct.unpack_from("<Q", image, offset + 24)[0]
            return cluster, size
    raise RuntimeError("allocation bitmap entry not found")


def find_free_cluster(image, geo, bitmap_cluster, bitmap_size):
    bitmap_offset = cluster_offset(geo, bitmap_cluster)
    for bit in range(geo["cluster_count"] - 1, 0, -1):
        byte_index = bit // 8
        if byte_index >= bitmap_size:
            continue
        if (image[bitmap_offset + byte_index] & (1 << (bit % 8))) == 0:
            return bit + 2
    raise RuntimeError("no free cluster found")


def corrupt_leaked_bitmap_cluster(path):
    image = bytearray(open(path, "rb").read())
    geo = exfat_geometry(image)
    bitmap_cluster, bitmap_size = bitmap_info(image, geo)
    cluster = find_free_cluster(image, geo, bitmap_cluster, bitmap_size)
    bitmap_offset = cluster_offset(geo, bitmap_cluster)
    bit = cluster - 2
    image[bitmap_offset + bit // 8] |= 1 << (bit % 8)
    open(path, "wb").write(image)
    return cluster


def corrupt_free_cluster_fat(path):
    image = bytearray(open(path, "rb").read())
    geo = exfat_geometry(image)
    bitmap_cluster, bitmap_size = bitmap_info(image, geo)
    cluster = find_free_cluster(image, geo, bitmap_cluster, bitmap_size)
    struct.pack_into("<I", image, geo["fat_offset"] + cluster * 4, 0xFFFFFFFF)
    open(path, "wb").write(image)
    return cluster


def run_fsck(args, disk):
    port = random.randint(24000, 29000)
    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_usr + os.pathsep + msys_ucrt + os.pathsep + env.get("PATH", "")
    command = [
        args.qemu,
        "-M", "q35",
        "-m", args.memory,
        "-cdrom", args.iso,
        "-boot", "d",
        "-serial", f"tcp:127.0.0.1:{port},server,nowait",
        "-drive", f"if=none,id=exfat,file={disk},format=raw",
        "-device", "ich9-ahci,id=ahci",
        "-device", "ide-hd,drive=exfat,bus=ahci.0",
        "-monitor", "none",
        "-no-reboot",
    ]
    output = b""
    process = subprocess.Popen(command, cwd=args.root, env=env,
        stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    try:
        sock = connect_serial(port, 15)
        sock.settimeout(0.3)
        output += read_until(sock, b"srv> ", args.boot_wait)
        send_serial(sock, "fsck /fat\n", args.send_delay)
        output += read_until(sock, b"srv> ", 15)
        output += read_for(sock, 1)
    finally:
        try:
            process.terminate()
            process.wait(timeout=3)
        except Exception:
            process.kill()
    return output.decode("utf-8", "replace")


def main():
    parser = argparse.ArgumentParser(description="Verify srvros fsck catches controlled exFAT corruption.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--send-delay", type=float, default=0.001)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    args.root = os.path.abspath(args.root)
    args.iso = args.iso if os.path.isabs(args.iso) else os.path.join(args.root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(args.root, args.disk)

    cases = [
        ("leak", corrupt_leaked_bitmap_cluster, "leaked allocated clusters="),
        ("fat", corrupt_free_cluster_fat, "free clusters with FAT entries="),
    ]
    all_output = []
    with tempfile.TemporaryDirectory(prefix="srvros-fsck-corrupt-") as temp_dir:
        for name, mutate, marker in cases:
            disk = os.path.join(temp_dir, f"{name}.exfat")
            shutil.copyfile(source_disk, disk)
            cluster = mutate(disk)
            text = run_fsck(args, disk)
            all_output.append(f"### case {name} cluster={cluster}\n{text}")
            if has_fatal_exception(text):
                print("\n".join(all_output))
                print(f"fsck-corrupt: fatal exception in {name}", file=sys.stderr)
                return 2
            if marker not in text or "errors=0 ok" in text:
                print("\n".join(all_output))
                print(f"fsck-corrupt: missing expected marker for {name}: {marker}", file=sys.stderr)
                return 3

    print("\n".join(all_output))
    print("fsck-corrupt: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
