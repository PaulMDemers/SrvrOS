#!/usr/bin/env python3
"""Build the read-only Ubuntu reference and compare a native Air6,1 SSD receipt."""

import hashlib
import os
from pathlib import Path
import re
import stat
import struct
import uuid
import zlib

SECTOR = 512
SECTORS = 236978176
SAMPLES = ((0, 34), (SECTORS - 33, 33), (2048, 1))
RECEIPT = "srvros-work/air61-ssd-probe.log"
PORT_RECEIPT = "srvros-work/air61-ssd-port-state.log"
PCI = "0000:04:00.0"
MODEL = "APPLE SSD SD0128F"
EFI = uuid.UUID("c12a7328-f81f-11d2-ba4b-00a0c93ec93b").bytes_le
IDENTITY = "air61-ssd-device: pci=1b4b:9183 revision=14 bdf=04:00.0 sectors=236978176 block_size=512"
PASS = "air61-ssd: result=PASS internal_mounted=no ata_writes=0"


def require(wanted, message):
    if not wanted:
        raise ValueError(message)


def verify_gpt(primary, backup):
    require(len(primary) == 34 * SECTOR and len(backup) == 33 * SECTOR, "Short GPT sample")
    require(primary[510:512] == b"\x55\xaa", "Missing protective MBR signature")
    entries = [primary[i:i + 16] for i in range(446, 510, 16)]
    used = [entry for entry in entries if any(entry)]
    require(len(used) == 1, "Missing or hybrid protective MBR")
    entry = used[0]
    start, length = struct.unpack_from("<II", entry, 8)
    require(entry[0] == 0 and entry[4] == 0xee and start == 1 and
            length in (SECTORS - 1, 0xffffffff), "Invalid protective MBR extent")

    primary_table, backup_table = primary[2 * SECTOR:], backup[:-SECTOR]
    require(primary_table == backup_table, "GPT entry arrays differ")
    entries_crc = zlib.crc32(primary_table)
    shared = []
    for header, here, other, table in (
        (primary[SECTOR:2 * SECTOR], 1, SECTORS - 1, 2),
        (backup[-SECTOR:], SECTORS - 1, 1, SECTORS - 33),
    ):
        revision, size, checksum, reserved = struct.unpack_from("<IIII", header, 8)
        require(header[:8] == b"EFI PART" and revision == 0x10000 and
                92 <= size <= SECTOR and reserved == 0, "Invalid GPT header")
        require(zlib.crc32(header[:16] + bytes(4) + header[20:size]) == checksum,
                "GPT header CRC mismatch")
        current, alternate, first, last, guid, table_lba, count, stride, crc = (
            struct.unpack_from("<QQQQ16sQIII", header, 24))
        require((current, alternate, table_lba) == (here, other, table) and
                34 <= first <= last < SECTORS - 33 and any(guid) and
                (count, stride, crc) == (128, 128, entries_crc),
                "Invalid GPT geometry or entry CRC")
        shared.append((first, last, guid))
    require(shared[0] == shared[1], "GPT headers disagree")
    first, last, _ = shared[0]
    partitions = []
    guids = set()
    for i in range(128):
        kind, identity, start, end = struct.unpack_from("<16s16sQQ", primary_table, i * 128)
        if not any(kind):
            continue
        require(any(identity) and identity not in guids and first <= start <= end <= last,
                "Invalid GPT partition identity or extent")
        guids.add(identity)
        partitions.append((start, end, kind))
    partitions.sort()
    require(partitions and partitions[0][::2] == (2048, EFI), "Expected EFI partition at LBA 2048")
    require(all(left[1] < right[0] for left, right in zip(partitions, partitions[1:])),
            "Overlapping GPT partitions")
    return len(partitions)


def read_samples(fd):
    data = [os.pread(fd, count * SECTOR, start * SECTOR) for start, count in SAMPLES]
    require(all(len(content) == count * SECTOR for (_, count), content in zip(SAMPLES, data)),
            "Short SSD read")
    partitions = verify_gpt(*data[:2])
    require(data[2][510:512] == b"\x55\xaa", "Missing ESP boot-sector signature")
    return {
        "gpt": "PASS",
        "partitions": partitions,
        "samples": [dict(lba=lba, sectors=count, sha256=hashlib.sha256(content).hexdigest())
                    for (lba, count), content in zip(SAMPLES, data)],
    }


def ubuntu_baseline(disk):
    disk = Path(disk).resolve(strict=True)
    model = Path("/sys/class/dmi/id/product_name").read_text().strip()
    require(model == "MacBookAir6,1", "Wrong machine model")
    sysfs = (Path("/sys/block") / disk.name).resolve(strict=True)
    device = Path("/sys/bus/pci/devices") / PCI
    require(device.resolve(strict=True) in sysfs.parents, "Disk is outside the captured Marvell controller")
    for name, expected in (("vendor", "0x1b4b"), ("device", "0x9183"), ("revision", "0x14"),
                           ("class", "0x010601")):
        require((device / name).read_text().strip() == expected, f"PCI {name} mismatch")
    # The SCSI model string is limited to 16 characters. Use the full ATA
    # IDENTIFY model from the sole link associated with this block device.
    ports = [parent for parent in sysfs.parents if re.fullmatch(r"ata[0-9]+", parent.name)]
    require(len(ports) == 1, "Could not bind the block device to one ATA port")
    targets = [target for target in Path("/sys/class/ata_device").iterdir()
               if target.resolve().is_relative_to(ports[0])]
    require(len(targets) == 1, "Ambiguous ATA device on the captured port")
    words = (targets[0] / "id").read_text().split()
    require(len(words) == 256, "Invalid ATA IDENTIFY data in sysfs")
    identify_model = bytes.fromhex("".join(words[27:47])).decode("ascii").rstrip()
    require(identify_model == MODEL and int((sysfs / "size").read_text()) == SECTORS and
            int((sysfs / "queue/logical_block_size").read_text()) == SECTOR,
            "SSD model or geometry mismatch")
    fd = os.open(disk, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
    try:
        info = os.fstat(fd)
        require(stat.S_ISBLK(info.st_mode) and
                f"{os.major(info.st_rdev)}:{os.minor(info.st_rdev)}" == (sysfs / "dev").read_text().strip(),
                "Opened device does not match sysfs")
        return read_samples(fd)
    finally:
        os.close(fd)


def probe_profile(release, volume, port_state):
    profile = ("air61-ssd: profile srvros.storage=ram srvros.capture=air61-ssd-read-only "
               f"srvros.workspace-volume={volume} srvros.ssd-release={release}")
    return profile + (" srvros.ssd-port-state=1" if port_state else "")


def read_port_trace(receipt, release, volume):
    lines = receipt.splitlines()
    require(lines.count(probe_profile(release, volume, True)) == 1, "Port trace release or workspace differs")
    controllers = [line for line in lines if line.startswith("air61-ssd-controller:")]
    require(len(controllers) == 1, "Missing or duplicate AHCI controller snapshot")
    match = re.fullmatch(r"air61-ssd-controller: firmware_ghc=0x0{8}([0-9a-f]{8}) firmware_port=([01])",
                         controllers[0])
    require(match is not None, "Malformed AHCI controller snapshot")
    ghc = int(match[1], 16)
    phases = ["before-stop", "after-stop", "buffers-programmed", "fis-enabled",
              "engine-started", "identify-return", "retire-return"]
    rows = [line for line in lines if line.startswith("air61-ssd-port:")]
    if match[2] == "1":
        require(ghc & (1 << 31), "Firmware port sampled with AHCI disabled")
        phases.insert(0, "firmware")
    require(len(rows) == len(phases), "Incomplete or duplicate AHCI port snapshots")
    registers = "ghc cmd tfd sig ssts sctl serr is ci sact clb fb".split()
    fields = ["phase", "port", "us", *registers, "fis_valid", "pio", "d2h", "sdb"]
    snapshots = []
    for line, phase in zip(rows, phases):
        pairs = [word.split("=", 1) for word in line.removeprefix("air61-ssd-port: ").split()]
        require([pair[0] for pair in pairs] == fields and all(len(pair) == 2 for pair in pairs),
                "Malformed AHCI port fields")
        row = dict(pairs)
        require(row["phase"] == phase, "AHCI port snapshots are out of order")
        for key in ("port", "us", "fis_valid"):
            require(re.fullmatch(r"0|[1-9][0-9]{0,19}", row[key]), "Malformed AHCI snapshot counter")
            row[key] = int(row[key])
        require(row["port"] < 32 and row["us"] < 2**64 and row["fis_valid"] in (0, 1),
                "AHCI snapshot counter outside its range")
        for key in (*registers, "pio", "d2h", "sdb"):
            mask = r"0x[0-9a-f]{16}" if key in ("clb", "fb") else r"0x0{8}[0-9a-f]{8}"
            require(re.fullmatch(mask, row[key]), "Malformed AHCI register width")
            row[key] = int(row[key], 16)
        if phase in ("firmware", "before-stop", "after-stop"):
            require(not row["fis_valid"], "Firmware FIS buffer was sampled")
        if not row["fis_valid"]:
            require(not any(row[key] for key in ("pio", "d2h", "sdb")), "Unowned FIS data reported")
        if phase == "firmware":
            require(row["ghc"] & (1 << 31), "Firmware port snapshot has AHCI disabled")
        if snapshots:
            require(row["us"] >= snapshots[-1]["us"] and row["port"] == snapshots[-1]["port"],
                    "AHCI port or clock changed during capture")
        snapshots.append(row)
    return dict(status="COMPLETE", firmware_ghc=ghc, snapshots=snapshots)


def compare_receipt(receipt, release, volume, baseline, *, port_state=False):
    profile = probe_profile(release, volume, port_state)
    rows = [line for line in receipt.splitlines()
            if line.startswith(("air61-ssd:", "air61-ssd-device:", "air61-ssd-sample:"))]
    require(baseline["gpt"] == "PASS", "Missing Ubuntu GPT validation")
    require(len(rows) == 6 and rows[:2] == [profile, IDENTITY] and rows[-1] == PASS,
            "Missing or mismatched native probe completion")
    samples = []
    for row in rows[2:-1]:
        match = re.fullmatch(r"air61-ssd-sample: lba=([0-9]+) sectors=([0-9]+) sha256=([0-9a-f]{64})",
                             row)
        require(match is not None, "Malformed native sample")
        samples.append(dict(lba=int(match[1]), sectors=int(match[2]), sha256=match[3]))
    require(tuple((sample["lba"], sample["sectors"]) for sample in samples) == SAMPLES,
            "Unexpected native sample coverage")
    require(samples == baseline["samples"], "Native sample hashes differ from Ubuntu")
    return dict(status="PASS", release=release, gpt="primary-and-backup-match-verified-ubuntu",
                partitions=baseline["partitions"], samples=samples, internal_mounted=False, ata_writes=0)
