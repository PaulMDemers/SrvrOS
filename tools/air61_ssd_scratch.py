#!/usr/bin/env python3
"""Build and collect the Air6,1 disposable SSD trial. Never opens a raw disk."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
from types import SimpleNamespace
import uuid
import zlib

from air61_ssd_probe import SECTOR, SECTORS, read_port_trace_for_profile, require, verify_gpt
from hardware_boot import EMPTY_INITRAMFS, compact_volume_uuid, digest
from mk_usb_image import ESP_TYPE, build_esp, gpt_entry, gpt_header

PAGE = 4096
ESP_LBA = 2048
ESP_BYTES = 272 * 1024 * 1024
SCRATCH_LBA = 600000
SCRATCH_SECTORS = 3 * PAGE // SECTOR
SCRATCH_TYPE = uuid.UUID("7a0365f1-b9a3-40ea-8e6a-d1e2ba824070")
IMAGE = "disposable-air61-ssd.raw"
PLAN = "fixture.json"


def gpt(disk_guid, esp_guid, scratch_guid):
    entries = (
        gpt_entry(ESP_TYPE, esp_guid, ESP_LBA, ESP_LBA + ESP_BYTES // SECTOR - 1, "srvros-esp") +
        gpt_entry(SCRATCH_TYPE, scratch_guid, SCRATCH_LBA, SCRATCH_LBA + SCRATCH_SECTORS - 1, "srvros-scratch")
    ).ljust(128 * 128, b"\0")
    last = SECTORS - 1
    crc = zlib.crc32(entries)
    mbr = bytearray(SECTOR)
    mbr[446:462] = struct.pack("<B3sB3sII", 0, b"\x00\x02\x00", 0xee, b"\xff\xff\xff", 1, last)
    mbr[510:] = b"\x55\xaa"
    primary = bytes(mbr) + gpt_header(1, last, 34, last - 33, 2, 128, crc, disk_guid) + entries
    backup = entries + gpt_header(last, 1, 34, last - 33, last - 32, 128, crc, disk_guid)
    require(verify_gpt(primary, backup) == 2, "Unexpected fixture GPT")
    return primary, backup


def page(seed, label):
    return b"".join(hashlib.sha256(seed + bytes((label, index))).digest() for index in range(PAGE // 32))


def profile(volume, fixture):
    return (f"srvros.storage=ram srvros.capture=air61-ssd-scratch srvros.workspace-volume={volume} "
            f"srvros.ssd-fixture={fixture}")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def scratch_pages(fixture, written=False):
    seed = bytes.fromhex(fixture[:64])
    return page(seed, 0) + page(seed, 3 if written else 1) + page(seed, 2)


def build(output, kernel, loader, volume):
    for source in (kernel, loader):
        require(source.is_file() and source.stat().st_size > 0, f"Missing regular boot input {source}")
    volume = compact_volume_uuid(volume)
    # Exclusive directory and file creation. There is no device-target argument.
    output.mkdir(mode=0o700)
    disk_guid, esp_guid, scratch_guid = (uuid.uuid4() for _ in range(3))
    primary, backup = gpt(disk_guid, esp_guid, scratch_guid)
    fixture = sha(primary) + sha(backup)
    staged_kernel = output / "srvros.elf"
    subprocess.run(("objcopy", "--strip-debug", kernel, staged_kernel), check=True)
    bootstrap = output / "initramfs.tar"
    bootstrap.write_bytes(EMPTY_INITRAMFS)
    config = output / "limine.conf"
    config.write_text("timeout: 0\nserial: yes\n\n"
                      "/Air6,1 disposable SSD durability\n"
                      "    protocol: limine\n"
                      "    path: boot():/boot/srvros.elf\n"
                      "    module_path: boot():/boot/initramfs.tar\n"
                      f"    cmdline: {profile(volume, fixture)}\n")
    esp = build_esp(SimpleNamespace(bootx64=loader, bootia32=None, kernel=staged_kernel,
                                    initramfs=bootstrap, config=config), ESP_BYTES, ESP_LBA)
    regions = ((0, primary), (ESP_LBA, esp), (SCRATCH_LBA, scratch_pages(fixture)), (SECTORS - 33, backup))
    with (output / IMAGE).open("xb") as stream:
        stream.truncate(SECTORS * SECTOR)
        for lba, content in regions:
            stream.seek(lba * SECTOR)
            # Preserve sparse zero ranges in the FAT image as well as outside it.
            for start in range(0, len(content), 1024 * 1024):
                chunk = content[start:start + 1024 * 1024]
                if any(chunk):
                    stream.write(chunk)
                else:
                    stream.seek(len(chunk), os.SEEK_CUR)
        stream.flush()
        os.fsync(stream.fileno())
    plan = dict(fixture=fixture, workspace_volume=volume, disk_guid=str(disk_guid),
                esp_guid=str(esp_guid), scratch_guid=str(scratch_guid),
                sectors=SECTORS, sector_bytes=SECTOR, write_lba=SCRATCH_LBA + PAGE // SECTOR,
                write_sectors=PAGE // SECTOR, written_pages_sha256=sha(scratch_pages(fixture, written=True)),
                kernel_sha256=digest(staged_kernel), loader_sha256=digest(loader),
                config_sha256=digest(config),
                regions=[dict(lba=lba, sectors=len(content) // SECTOR, sha256=sha(content))
                         for lba, content in regions])
    verify_image(output / IMAGE, plan)
    with (output / PLAN).open("x") as stream:
        json.dump(plan, stream, indent=2)
        stream.write("\n")
    return plan


def verify_image(image, plan):
    require(stat.S_ISREG(image.lstat().st_mode), "Expected a regular fixture image")
    require(plan["sectors"] == SECTORS and image.stat().st_size == SECTORS * SECTOR,
            "Fixture image size differs")
    require([(region["lba"], region["sectors"]) for region in plan["regions"]] ==
            [(0, 34), (ESP_LBA, ESP_BYTES // SECTOR), (SCRATCH_LBA, SCRATCH_SECTORS), (SECTORS - 33, 33)],
            "Fixture region layout differs")
    with image.open("rb") as stream:
        for region in plan["regions"]:
            stream.seek(region["lba"] * SECTOR)
            remaining = region["sectors"] * SECTOR
            hasher = hashlib.sha256()
            while remaining:
                chunk = stream.read(min(remaining, 1024 * 1024))
                require(bool(chunk), "Fixture region ended early")
                hasher.update(chunk)
                remaining -= len(chunk)
            require(hasher.hexdigest() == region["sha256"], f"Fixture bytes changed at LBA {region['lba']}")


def read_log(root, fixture, phase):
    path = root / f"air61-ssd-{fixture[:24]}-{phase}.log"
    require(stat.S_ISREG(path.lstat().st_mode), f"{phase} receipt is not a regular file")
    with path.open("rb") as stream:
        data = stream.read(65537)
    require(0 < len(data) <= 65536, f"Invalid {phase} receipt length")
    return data.decode("ascii", errors="replace")


def compare_log(receipt, plan, phase):
    cmdline = profile(plan["workspace_volume"], plan["fixture"])
    expected = f"air61-scratch: profile {cmdline}"
    relevant = [line for line in receipt.splitlines() if line.startswith("air61-scratch:")]
    if phase == "armed":
        require(relevant == [expected, "air61-scratch: phase=write"], "Invalid attempt marker")
        return
    require(phase in ("write", "verify"), "Unknown receipt phase")
    writes, flushes = (PAGE // SECTOR, 1) if phase == "write" else (0, 0)
    require(relevant == [
        expected, f"air61-scratch: phase={phase}",
        f"air61-scratch: pages-sha256={plan['written_pages_sha256']} "
        f"lba={SCRATCH_LBA + PAGE // SECTOR} sectors={PAGE // SECTOR} writes={writes} flushes={flushes}",
        "air61-scratch: result=PASS internal_mounted=no",
    ], f"{phase} result, digest, or command counts differ")
    trace = read_port_trace_for_profile(receipt, expected, reset=True)
    end = trace["snapshots"][-1]
    require(trace["reset"]["status"] == "ready" and not (end["cmd"] & 0xc011 or end["ci"] or end["sact"]),
            f"{phase} did not prove AHCI reset and retirement")
    return trace["reset"]


def collect(output, receipts):
    plan = json.loads((output / PLAN).read_text())
    fixture = plan["fixture"]
    require(re.fullmatch("[0-9a-f]{128}", fixture) is not None, "Invalid fixture ID")
    primary, backup = gpt(*(uuid.UUID(plan[key]) for key in ("disk_guid", "esp_guid", "scratch_guid")))
    require(fixture == sha(primary) + sha(backup) and
            plan["written_pages_sha256"] == sha(scratch_pages(fixture, written=True)),
            "Fixture ID or expected write digest differs")
    verify_image(output / IMAGE, plan)
    check = {}
    for phase in ("armed", "write", "verify"):
        check[phase] = compare_log(read_log(receipts, fixture, phase), plan, phase)
    return dict(status="PASS", fixture=fixture, write_bytes=PAGE, flush_commands=1,
                verify_write_commands=0, pages_sha256=plan["written_pages_sha256"],
                reset={phase: check[phase] for phase in ("write", "verify")})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest="action", required=True)
    make = actions.add_parser("build")
    make.add_argument("output", type=Path)
    make.add_argument("--kernel", type=Path, required=True)
    make.add_argument("--loader", type=Path, required=True)
    make.add_argument("--workspace-volume", required=True)
    verify = actions.add_parser("collect")
    verify.add_argument("output", type=Path)
    verify.add_argument("receipts", type=Path, help="Mounted evidence USB's srvros-work directory")
    args = parser.parse_args()
    if args.action == "build":
        result = build(args.output, args.kernel, args.loader, args.workspace_volume)
    else:
        result = collect(args.output, args.receipts)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        sys.exit(f"Air6,1 SSD scratch trial: {exc}")
