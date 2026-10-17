#!/usr/bin/env python3
"""Exercise the staged GRUB -> Limine -> kernel path on a disposable QEMU ESP."""
import argparse
import binascii
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import uuid

import hardware_boot as boot
from mk_usb_image import Fat32Builder, ESP_TYPE, gpt_entry, gpt_header
from uefi_usb_smoke import default_ovmf_path, qmp_command, send_qmp_text, has_fatal_exception


def make_disk(path, loader, source, release):
    # Same paths and filesystem UUID search as the host entry; only the UUID differs.
    size = 384 * 1024 * 1024
    first, sectors = 2048, size // 512
    last = first + sectors - 1
    backup = last + 2048
    fat = Fat32Builder(size, first)
    efi = fat.add_dir(fat.root_cluster, "EFI")
    fallback = fat.add_dir(efi, "BOOT")
    fat.add_file(fallback, "BOOTX64.EFI", loader.read_bytes())
    srvros = fat.add_dir(efi, "srvros")
    payload = fat.add_dir(srvros, release)
    loader_dir = fat.add_dir(payload, "loader")
    for name in boot.FILES:
        parent = loader_dir if name.startswith("loader/") else payload
        fat.add_file(parent, Path(name).name, (source / name).read_bytes())
    entries = bytearray(128 * 128)
    entries[:128] = gpt_entry(ESP_TYPE, uuid.uuid4(), first, last, "SrvrOS test ESP")
    crc, guid = binascii.crc32(entries) & 0xffffffff, uuid.uuid4()
    mbr = bytearray(512)
    mbr[446:462] = struct.pack("<B3sB3sII", 0, b"\x00\x02\x00", 0xee, b"\xff\xff\xff", 1, backup)
    mbr[510:512] = b"\x55\xaa"
    with path.open("wb") as stream:
        stream.truncate((backup + 1) * 512)
        stream.write(mbr)
        stream.write(gpt_header(1, backup, 34, backup - 33, 2, 128, crc, guid))
        stream.write(entries)
        stream.seek(first * 512)
        stream.write(fat.build())
        stream.seek((backup - 32) * 512)
        stream.write(entries)
        stream.seek(backup * 512)
        stream.write(gpt_header(backup, 1, 34, backup - 33, backup - 32, 128, crc, guid))


def wait_for(log, predicate, process, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        text = log.read_text(errors="replace") if log.exists() else ""
        if predicate(text):
            return text
        if process.poll() is not None:
            raise RuntimeError(f"QEMU exited with {process.returncode}; see {log.parent}")
        time.sleep(0.2)
    raise RuntimeError(f"Timed out; see {log}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boot-wait", type=float, default=180)
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--ovmf", default=default_ovmf_path())
    args = parser.parse_args()
    release = (boot.STAGE / "latest").read_text().strip()
    source = boot.STAGE / release
    boot.verify(source)
    logs = boot.ROOT / "build/review"
    logs.mkdir(parents=True, exist_ok=True)
    serial = logs / "hardware-grub-serial.log"
    serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix="srvros-grub-") as tmp:
        temp = Path(tmp)
        cfg = temp / "grub.cfg"
        cfg.write_text("serial --unit=0 --speed=115200\nterminal_output console serial\n"
                       "echo SRVROS_GRUB_CHAINLOAD_TEST\nset default=srvros-current\n" +
                       boot.grub_config("5352-5652", release))
        subprocess.run(["grub-script-check", str(cfg)], check=True)
        loader = temp / "BOOTX64.EFI"
        subprocess.run(["grub-mkstandalone", "-O", "x86_64-efi", "--locales=", "--fonts=",
                        "--modules=part_gpt fat chain search search_fs_uuid normal serial terminal",
                        "-o", str(loader), f"boot/grub/grub.cfg={cfg}"], check=True)
        disk = temp / "esp.img"
        make_disk(disk, loader, source, release)
        qmp_path = temp / "qmp.sock"
        command = [args.qemu, "-M", "q35", "-m", "2G",
                   "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf}",
                   "-drive", f"format=raw,file={disk}",
                   "-device", "qemu-xhci,id=xhci", "-device", "usb-kbd,bus=xhci.0",
                   "-serial", f"file:{serial}", "-qmp", f"unix:{qmp_path},server=on,wait=off",
                   "-monitor", "none", "-display", "none", "-no-reboot"]
        with (logs / "hardware-grub-qemu.log").open("wb") as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                text = wait_for(serial, lambda s: s.endswith("srv> "), process, args.boot_wait)
                for marker in ("SRVROS_GRUB_CHAINLOAD_TEST", "srvros: entering kernel",
                               "lpss-spi: disabled (experimental transport)",
                               "storage: RAM-only mode; AHCI disabled",
                               "exfat: mounted /fat from initramfs-exfat"):
                    if marker not in text:
                        raise RuntimeError(f"Missing boot marker: {marker}")
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as qmp:
                    qmp.settimeout(5)
                    qmp.connect(str(qmp_path))
                    qmp.recv(4096)
                    qmp_command(qmp, {"execute": "qmp_capabilities"})
                    send_qmp_text(qmp, "echo grub-usb-ok\n", 0.08)
                    text = wait_for(serial, lambda s: "\ngrub-usb-ok\n" in s, process, 20)
                if has_fatal_exception(text):
                    raise RuntimeError("Fatal kernel exception in chainload test")
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    print(f"hardware-boot-smoke: PASS ({release}); GRUB chainload, RAM filesystem, USB input")
    print(f"Serial log: {serial}")


if __name__ == "__main__":
    main()
