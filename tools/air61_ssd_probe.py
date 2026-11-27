#!/usr/bin/env python3
"""Build the read-only Ubuntu reference and compare a native Air6,1 SSD receipt."""

import hashlib
import mmap
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import uuid
import zlib

SECTOR = 512
SECTORS = 236978176
SAMPLES = ((0, 34), (SECTORS - 33, 33), (2048, 1))
RECEIPT = "srvros-work/air61-ssd-probe.log"
PORT_RECEIPT = "srvros-work/air61-ssd-port-state.log"
RESET_RECEIPT = "srvros-work/air61-ssd-port-reset.log"
PARTITION_RECEIPT = "srvros-work/air61-ssd-partition-read.log"
DISCOVERY_RECEIPT = "srvros-work/air61-ssd-discovery.log"
DISCOVERY_COLD_RECEIPT = "srvros-work/air61-ssd-discovery-cold.log"
DISCOVERY_PROOF = "air61-ssd-discovery: initializer=normal device=ahci0 read_only=yes"
PREVIOUS_RESET_RECEIPT = "srvros-work/air61-ssd-port-reset-mounted-baseline.log"
PCI = "0000:04:00.0"
MODEL = "APPLE SSD SD0128F"
EFI = uuid.UUID("c12a7328-f81f-11d2-ba4b-00a0c93ec93b").bytes_le
IDENTITY = "air61-ssd-device: pci=1b4b:9183 revision=14 bdf=04:00.0 sectors=236978176 block_size=512"
PASS = "air61-ssd: result=PASS internal_mounted=no ata_writes=0"


def require(wanted, message):
    if not wanted:
        raise ValueError(message)


def _gpt_layout(primary, backup):
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
    ordered = sorted(partitions)
    require(ordered and ordered[0][::2] == (2048, EFI), "Expected EFI partition at LBA 2048")
    require(all(left[1] < right[0] for left, right in zip(ordered, ordered[1:])),
            "Overlapping GPT partitions")
    return partitions


def verify_gpt(primary, backup):
    return len(_gpt_layout(primary, backup))


def _direct_read(fd, size, offset):
    # The SSD has 4 KiB physical sectors. Read aligned windows and retain
    # exactly the requested slice for the GPT and boot-sector hashes.
    alignment = 4096
    start = offset // alignment * alignment
    skip = offset - start
    capacity = (skip + size + alignment - 1) // alignment * alignment
    with mmap.mmap(-1, capacity) as buffer:
        require(os.preadv(fd, [buffer], start) == capacity, "Short direct SSD read")
        return buffer[skip:skip + size]


def _read_samples(fd, *, direct=False, partition=False):
    read = _direct_read if direct else os.pread
    data = [read(fd, count * SECTOR, start * SECTOR) for start, count in SAMPLES]
    require(all(len(content) == count * SECTOR for (_, count), content in zip(SAMPLES, data)),
            "Short SSD read")
    layout = _gpt_layout(*data[:2])
    require(data[2][510:512] == b"\x55\xaa", "Missing ESP boot-sector signature")
    report = {
        "gpt": "PASS",
        "partitions": len(layout),
        "samples": [dict(lba=lba, sectors=count, sha256=hashlib.sha256(content).hexdigest())
                    for (lba, count), content in zip(SAMPLES, data)],
    }
    if partition:
        efi = [(start, end - start + 1) for start, end, kind in layout if kind == EFI]
        require(len(efi) == 1 and efi[0][1] >= 56, "Expected one EFI partition with room for sample ranges")
        start_lba, sectors = efi[0]
        report["partition_layout"] = [dict(lba=start, sectors=end - start + 1, efi=int(kind == EFI))
                                      for start, end, kind in layout]
        samples = []
        for offset, count in ((0, 8), (32, 16), (sectors - 8, 8)):
            content = read(fd, count * SECTOR, (start_lba + offset) * SECTOR)
            require(len(content) == count * SECTOR, "Short EFI partition read")
            if offset == 0:
                require(content[:SECTOR] == data[2], "EFI boot sector changed during capture")
            samples.append(dict(offset=offset, sectors=count, commands=count // 8,
                                sha256=hashlib.sha256(content).hexdigest()))
        report["partition_samples"] = samples
    return report, data[2]


def read_samples(fd):
    report, _ = _read_samples(fd)
    return report


def _ubuntu_capture(disk, *, direct=False, partition=False):
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
    flags = os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW
    if direct:
        flags |= os.O_DIRECT
    fd = os.open(disk, flags)
    try:
        info = os.fstat(fd)
        require(stat.S_ISBLK(info.st_mode) and
                f"{os.major(info.st_rdev)}:{os.minor(info.st_rdev)}" == (sysfs / "dev").read_text().strip(),
                "Opened device does not match sysfs")
        return _read_samples(fd, direct=direct, partition=partition)
    finally:
        os.close(fd)


def ubuntu_baseline(disk):
    report, _ = _ubuntu_capture(disk)
    return report


def _find_efi(*args):
    result = subprocess.run(("findmnt", "-rn", *args), capture_output=True, text=True, timeout=10)
    require(result.returncode in (0, 1) and not result.stderr, "Could not verify EFI mount state")
    return result.stdout.splitlines()


def _efi_mount(disk, mountpoint):
    rows = _find_efi("-M", str(mountpoint), "-o", "SOURCE,FSTYPE,OPTIONS")
    require(len(rows) == 1, "Expected a single mounted EFI filesystem")
    fields = rows[0].split()
    require(len(fields) == 3 and fields[1] == "vfat" and "rw" in fields[2].split(","),
            "Expected the normal writable Ubuntu EFI mount")
    partition = Path(fields[0]).resolve(strict=True)
    sysfs = Path("/sys/class/block")
    identity = (sysfs / partition.name).resolve(strict=True)
    backing = (sysfs / Path(disk).resolve(strict=True).name).resolve(strict=True)
    require(identity.parent == backing and int((identity / "start").read_text()) == SAMPLES[2][0],
            "EFI mount does not match the SSD boot-sector sample")
    fstab = _find_efi("--fstab", "--evaluate", "-M", str(mountpoint), "-o", "SOURCE,FSTYPE")
    fields_fstab = fstab[0].split() if len(fstab) == 1 else []
    require(len(fields_fstab) == 2 and fields_fstab[1] == "vfat" and
            Path(fields_fstab[0]).resolve(strict=True) == partition,
            "EFI fstab entry does not match the mounted partition")
    require(_find_efi("-S", str(partition), "-o", "TARGET") == [str(mountpoint)],
            "EFI partition has another mount")
    return partition, frozenset(fields[2].split(","))


def clean_ubuntu_reference(disk, mountpoint=Path("/boot/efi"), *, partition=False):
    """Capture the whole sector in each mount state and restore Ubuntu's EFI mount."""
    efi_partition, options = _efi_mount(disk, mountpoint)
    # The raw disk and its mounted partition can have different page-cache
    # entries. Bypass the raw-device cache on both sides of the clean unmount.
    mounted, mounted_sector = _ubuntu_capture(disk, direct=True)
    try:
        subprocess.run(("umount", "--", str(mountpoint)), check=True, timeout=20)
        require(not _find_efi("-M", str(mountpoint)) and not _find_efi("-S", str(efi_partition)),
                "EFI partition is still mounted")
        clean, clean_sector = _ubuntu_capture(disk, direct=True, partition=partition)
        require(not _find_efi("-M", str(mountpoint)) and not _find_efi("-S", str(efi_partition)),
                "EFI partition was mounted during the clean capture")
        require(clean["samples"][:2] == mounted["samples"][:2], "GPT changed across the EFI unmount")
    finally:
        if not _find_efi("-M", str(mountpoint)):
            require(not _find_efi("-S", str(efi_partition)), "EFI moved to another mount during capture")
            subprocess.run(("mount", "--", str(mountpoint)), check=True, timeout=20)
        require(_efi_mount(disk, mountpoint) == (efi_partition, options), "Ubuntu EFI mount was not restored")
    return clean, clean_sector, mounted_sector


def probe_profile(release, volume, port_state, reset=False, partition=False, discovery=False):
    profile = ("air61-ssd: profile srvros.storage=ram srvros.capture=air61-ssd-read-only "
               f"srvros.workspace-volume={volume} srvros.ssd-release={release}")
    if port_state or reset:
        profile += " srvros.ssd-port-state=1"
    if reset:
        profile += " srvros.ssd-reset=port0"
    if partition:
        profile += " srvros.ssd-partition-read=1"
    if discovery:
        profile += " srvros.ssd-discovery=normal"
    return profile


def read_reset_status(lines):
    rows = [line for line in lines if line.startswith("air61-ssd-reset:")]
    require(len(rows) == 1, "Missing or duplicate SATA reset status")
    match = re.fullmatch(r"air61-ssd-reset: result=(ready|refused|pulse-unproved|timeout|"
                         r"release-unproved|ownership-lost) attempts=([01]) hold_us=([0-9]{1,20}) "
                         r"wait_us=([0-9]{1,20})", rows[0])
    require(match is not None, "Malformed SATA reset status")
    status, attempts, hold, wait = match[1], *map(int, match.groups()[1:])
    require(bool(attempts) == (status != "refused") and hold < 2**64 and wait < 2**64,
            "Invalid SATA reset counters")
    if status in ("ready", "timeout"):
        require(hold >= 1000, "Unproved SATA reset hold")
    return dict(status=status, attempts=attempts, hold_us=hold, wait_us=wait)


def verify_reset_trace(status, snapshots):
    stages = {row["phase"]: row for row in snapshots}
    in_reset = [stages["fis-enabled"], stages["reset-return"]]
    if status["attempts"]:
        in_reset.extend((stages["reset-asserted"], stages["reset-released"]))
    require(all(row["port"] == 0 and not row["cmd"] & 0x8001 and not (row["ci"] | row["sact"])
                for row in in_reset), "Unsafe port state around SATA reset")
    if status["status"] != "ready":
        return
    before, asserted, released, ready = (stages[phase] for phase in
        ("fis-enabled", "reset-asserted", "reset-released", "reset-return"))
    control = before["sctl"]
    require(not control & 0xf and asserted["sctl"] == (control | 1) and
            released["sctl"] == ready["sctl"] == control, "SATA reset pulse or restore differs")
    require(all(row["fis_valid"] and row["fb"] == before["fb"] for row in in_reset) and
            not before["d2h"] and not asserted["d2h"], "Unproved fresh device status")
    d2h = ready["d2h"]
    ata_status = (d2h >> 16) & 0xff
    require(d2h & 0xfff == 0x34 and ata_status and not ata_status & 0xa9 and ready["sig"] == 0x101 and
            ready["ssts"] & 0xf0f == 0x103 and not ready["tfd"] & 0x88 and status["wait_us"] < 2000000,
            "Missing ready post-reset device status")


def read_port_trace(receipt, release, volume, *, reset=False, partition=False, discovery=False):
    lines = receipt.splitlines()
    require(lines.count(probe_profile(release, volume, True, reset, partition, discovery)) == 1,
            "Port trace release or workspace differs")
    controllers = [line for line in lines if line.startswith("air61-ssd-controller:")]
    require(len(controllers) == 1, "Missing or duplicate AHCI controller snapshot")
    match = re.fullmatch(r"air61-ssd-controller: firmware_ghc=0x0{8}([0-9a-f]{8}) firmware_port=([01])",
                         controllers[0])
    require(match is not None, "Malformed AHCI controller snapshot")
    ghc = int(match[1], 16)
    phases = ["before-stop", "after-stop", "buffers-programmed", "fis-enabled"]
    reset_status = read_reset_status(lines) if reset else None
    if reset_status:
        if reset_status["attempts"]:
            phases.extend(("reset-asserted", "reset-released"))
        phases.append("reset-return")
    if not reset_status or reset_status["status"] == "ready":
        phases.extend(("engine-started", "identify-return"))
    phases.append("retire-return")
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
    result = dict(status="COMPLETE", firmware_ghc=ghc, snapshots=snapshots)
    if reset_status:
        verify_reset_trace(reset_status, snapshots)
        result["reset"] = reset_status
    return result


def compare_receipt(receipt, release, volume, baseline, *, port_state=False, reset=False, partition=False,
                    discovery=False):
    profile = probe_profile(release, volume, port_state, reset, partition, discovery)
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
    result = dict(status="PASS", release=release, gpt="primary-and-backup-match-verified-ubuntu",
                  partitions=baseline["partitions"], samples=samples, internal_mounted=False, ata_writes=0)
    if discovery:
        require(partition and reset, "Discovery requires the partition and reset profile")
        proof = [line for line in receipt.splitlines() if line.startswith("air61-ssd-discovery:")]
        require(proof == [DISCOVERY_PROOF], "Missing or invalid read-only normal AHCI discovery evidence")
        result["initializer"] = "normal"
    if partition:
        require(reset, "Partition samples require the reset profile")
        trace = read_port_trace(receipt, release, volume, reset=True, partition=True, discovery=discovery)
        retired = trace["snapshots"][-1]
        require(trace["reset"]["status"] == "ready" and not retired["cmd"] & 0xc011 and
                not (retired["ci"] | retired["sact"]), "Unproved AHCI reset or retirement")
        gpt_rows = [line for line in receipt.splitlines() if line.startswith("air61-ssd-gpt:")]
        require(gpt_rows == ["air61-ssd-gpt: lba={lba} sectors={sectors} efi={efi}".format(**entry)
                             for entry in baseline["partition_layout"]], "Native GPT partition layout differs")
        partition_rows = [line for line in receipt.splitlines() if line.startswith("air61-ssd-partition:")]
        require(partition_rows == [
            "air61-ssd-partition: offset={offset} sectors={sectors} commands={commands} sha256={sha256}".format(**sample)
            for sample in baseline["partition_samples"]
        ], "Native EFI partition samples or command counts differ")
        result.update(partition_layout=baseline["partition_layout"], partition_samples=baseline["partition_samples"],
                      partition_read_commands=sum(sample["commands"] for sample in baseline["partition_samples"]))
    return result
