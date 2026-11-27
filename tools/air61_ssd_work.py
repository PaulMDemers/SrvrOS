#!/usr/bin/env python3
"""Seed and verify the bounded Air6,1 native SSD test from installed Ubuntu."""

import argparse
import json
import mmap
import os
from pathlib import Path
import struct
import subprocess
import sys
import uuid

from air61_ssd_probe import _direct_read, verify_gpt
from air61_ssd_scratch import PAGE, compare_log, read_log, scratch_pages, sha
from air61_work_layout import (
    EFI, ROOT_NEW, ROOT_START, ROOT_TYPE, SECTOR, SECTORS, WORK_NAME, WORK_SECTORS, WORK_START,
    WORK_TYPE, command, device_number, disk_for_plan, durable_text, layout, read_plan, require,
)

OFFSET = 1024 * 1024
WRITE_LBA = WORK_START + (OFFSET + PAGE) // SECTOR


def verified_gpt(primary, backup, plan):
    require(verify_gpt(primary, backup) == 3, "Expected exactly three GPT partitions")
    root = plan["ubuntu_partition"]
    scratch = plan["scratch_partition"]
    require(uuid.UUID(bytes_le=primary[SECTOR + 56:SECTOR + 72]) == uuid.UUID(plan["disk_guid"]),
            "Raw GPT disk GUID differs")
    entries = primary[2 * SECTOR:]
    expected = [(*EFI, plan["efi_partuuid"]), (ROOT_START, ROOT_NEW, ROOT_TYPE, root["partuuid"]),
                (WORK_START, WORK_SECTORS, WORK_TYPE, scratch["partuuid"])]
    for i, (first, size, kind, identity) in enumerate(expected):
        type_bytes, id_bytes, begin, end = struct.unpack_from("<16s16sQQ", entries, i * 128)
        require((uuid.UUID(bytes_le=type_bytes), uuid.UUID(bytes_le=id_bytes), begin, end) ==
                (uuid.UUID(kind), uuid.UUID(identity), first, first + size - 1),
                f"Raw GPT partition {i + 1} differs")
    require(entries[2 * 128 + 56:3 * 128].decode("utf-16-le").rstrip("\0") == WORK_NAME,
            "Raw GPT work label differs")
    return sha(primary) + sha(backup)


def check_open(fd, path):
    st = os.fstat(fd)
    require(f"{os.major(st.st_rdev)}:{os.minor(st.st_rdev)}" == device_number(path),
            "Opened device differs")


def gpt_regions(disk):
    fd = os.open(disk, os.O_RDONLY | os.O_DIRECT | os.O_CLOEXEC | os.O_NOFOLLOW)
    try:
        check_open(fd, disk)
        return _direct_read(fd, 34 * SECTOR, 0), _direct_read(fd, 33 * SECTOR, (SECTORS - 33) * SECTOR)
    finally:
        os.close(fd)


def scratch_device(plan):
    disk = disk_for_plan(plan)
    parts = layout(disk, plan, True)
    scratch = (Path("/dev/disk/by-partuuid") / plan["scratch_partition"]["partuuid"]).resolve(strict=True)
    require(scratch == Path(parts[2]["node"]), "Work device does not match the table")
    sys_part = (Path("/sys/class/block") / scratch.name).resolve(strict=True)
    require(sys_part.parent == (Path("/sys/block") / disk.name).resolve(strict=True) and
            (sys_part / "dev").read_text().strip() == device_number(scratch) and
            int((sys_part / "start").read_text()) == WORK_START and
            int((sys_part / "size").read_text()) == WORK_SECTORS and
            int((sys_part / "partition").read_text()) == 3 and
            not any((sys_part / "holders").iterdir()), "Work device geometry or ownership differs")
    return disk, scratch


def receipt_dir(root, plan):
    root = root.resolve(strict=True)
    usb = (Path("/dev/disk/by-uuid") / plan["evidence_uuid"]).resolve(strict=True)
    require(root.is_dir() and root.name == "srvros-work" and
            command("findmnt", "-T", root, "-n", "-o", "MAJ:MIN").strip() == device_number(usb),
            "Receipts must be on the configured evidence USB")
    return root


def run(plan, output, receipts, action):
    require(os.geteuid() == 0, "Use sudo to read the verified SSD partition directly")
    disk, scratch = scratch_device(plan)
    primary, backup = gpt_regions(disk)
    proof = verified_gpt(primary, backup, plan)
    receipts = receipt_dir(receipts, plan)
    expected = dict(fixture=proof, workspace_volume=plan["evidence_uuid"].replace("-", "").lower(),
                    written_pages_sha256=sha(scratch_pages(proof, written=True)), write_lba=WRITE_LBA)
    flags = os.O_EXCL | os.O_DIRECT | os.O_CLOEXEC | os.O_NOFOLLOW
    if action == "seed":
        require(not list(receipts.glob(f"air61-ssd-{proof[:24]}-*.log")), "Evidence for this GPT already exists")
        require(subprocess.run(("blkid", "-p", scratch), capture_output=True).returncode == 2,
                "Work partition has a signature or could not be checked")
        output.mkdir(mode=0o700)
        for name, data in (("primary.bin", primary), ("backup.bin", backup)):
            with (output / name).open("xb") as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
        durable_text(output / "trial.json", json.dumps(expected, indent=2) + "\n")
        fd = os.open(scratch, os.O_RDWR | flags)
        try:
            check_open(fd, scratch)
            with mmap.mmap(-1, 3 * PAGE) as buffer:
                buffer[:] = scratch_pages(proof)
                require(os.pwritev(fd, [buffer], OFFSET) == len(buffer), "Short initial fixture write")
            os.fsync(fd)
        finally:
            os.close(fd)
    else:
        require(json.loads((output / "trial.json").read_text()) == expected and
                primary == (output / "primary.bin").read_bytes() and backup == (output / "backup.bin").read_bytes(),
                "SSD GPT or trial record changed since seeding")
        phases = ("armed", "write", "verify") if action == "collect" else ("armed", "write")
        for phase in phases:
            compare_log(read_log(receipts, proof, phase), expected, phase, internal=True)
        require(not list(receipts.glob(f"air61-ssd-{proof[:24]}-*-failed.log")), "A failure receipt exists")
    fd = os.open(scratch, os.O_RDONLY | flags)
    try:
        check_open(fd, scratch)
        require(_direct_read(fd, 3 * PAGE, OFFSET) == scratch_pages(proof, written=action != "seed"),
                "SSD guard or payload readback differs")
    finally:
        os.close(fd)
    require(gpt_regions(disk) == (primary, backup), "GPT changed during host checks")
    result = dict(**expected, action=action, status="PASS", native_write_accepted=action == "collect")
    durable_text(output / f"{action}-result.json", json.dumps(result, indent=2) + "\n")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("seed", "check-write", "collect"))
    parser.add_argument("plan", type=Path)
    parser.add_argument("output", type=Path, help="New persistent trial directory for seed")
    parser.add_argument("receipts", type=Path, help="Mounted evidence USB's srvros-work directory")
    args = parser.parse_args()
    print(json.dumps(run(read_plan(args.plan), args.output, args.receipts, args.action), indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as exc:
        sys.exit(f"Air6,1 SSD work trial: {exc}")
