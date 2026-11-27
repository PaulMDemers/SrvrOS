#!/usr/bin/env python3
"""Prepare the exact Air6,1 work partition from an Ubuntu live USB."""

import argparse
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import uuid

MODEL = "MacBookAir6,1"
SSD = "APPLE SSD SD0128F"
SECTOR = 512
SECTORS = 236978176
EFI = (2048, 2201600, "c12a7328-f81f-11d2-ba4b-00a0c93ec93b")
ROOT_START = 2203648
ROOT_OLD = 234772480
ROOT_NEW = 226383872
ROOT_TYPE = "0fc63daf-8483-4772-8e79-3d69d8477de4"
WORK_START = ROOT_START + ROOT_NEW
WORK_SECTORS = 8388608
WORK_TYPE = "7a0365f1-b9a3-40ea-8e6a-d1e2ba824070"
WORK_NAME = "srvros-work"
PCI = "0000:04:00.0"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def command(*args, script=None, accepted=(0,)):
    result = subprocess.run(args, input=script, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, env={**os.environ, "LC_ALL": "C"})
    require(result.returncode in accepted, f"{' '.join(map(str, args))} failed ({result.returncode})\n"
            f"{result.stdout}{result.stderr}")
    return result.stdout


def read_plan(path):
    plan = json.loads(path.read_text())
    root = plan["ubuntu_partition"]
    work = plan["scratch_partition"]
    require((plan["model"], plan["model_from_linux"], plan["logical_sector_bytes"], plan["sectors"]) ==
            (MODEL, SSD, SECTOR, SECTORS), "Wrong SSD plan")
    require((root["first_sector"], root["current_sectors"], root["new_sectors"], root["new_last_sector"]) ==
            (ROOT_START, ROOT_OLD, ROOT_NEW, WORK_START - 1), "Wrong Ubuntu geometry in plan")
    require((work["name"], work["first_sector"], work["last_sector"], work["sectors"], work["type"]) ==
            (WORK_NAME, WORK_START, WORK_START + WORK_SECTORS - 1, WORK_SECTORS, WORK_TYPE),
            "Wrong work geometry in plan")
    ids = [plan["disk_guid"], plan["efi_partuuid"], root["partuuid"], root["filesystem_uuid"], work["partuuid"]]
    require(all(str(uuid.UUID(value)) == value for value in ids) and len(set(ids)) == len(ids),
            "Invalid or duplicate identities in plan")
    require(re.fullmatch(r"[0-9A-F]{4}-[0-9A-F]{4}", plan["evidence_uuid"]), "Invalid evidence USB UUID")
    return plan


def device_number(path):
    info = path.stat()
    require(stat.S_ISBLK(info.st_mode), f"Expected block device {path}")
    return f"{os.major(info.st_rdev)}:{os.minor(info.st_rdev)}"


def disk_for_plan(plan):
    require(Path("/sys/class/dmi/id/product_name").read_text().strip() == MODEL, "Wrong Mac model")
    root = (Path("/dev/disk/by-partuuid") / plan["ubuntu_partition"]["partuuid"]).resolve(strict=True)
    sysroot = (Path("/sys/class/block") / root.name).resolve(strict=True)
    disk = Path("/dev") / sysroot.parent.name
    require(re.fullmatch(r"sd[a-z]+", disk.name) and root.name == disk.name + "2", "Unexpected Ubuntu device")
    sysdisk = (Path("/sys/block") / disk.name).resolve(strict=True)
    controller = (Path("/sys/bus/pci/devices") / PCI).resolve(strict=True)
    require(controller in sysdisk.parents, "SSD is outside the captured Marvell controller")
    for key, expected in (("vendor", "0x1b4b"), ("device", "0x9183"), ("revision", "0x14"), ("class", "0x010601")):
        require((controller / key).read_text().strip() == expected, f"Unexpected controller {key}")
    ports = [parent for parent in sysdisk.parents if re.fullmatch(r"ata[0-9]+", parent.name)]
    require(len(ports) == 1, "Could not identify the ATA port")
    targets = [entry for entry in Path("/sys/class/ata_device").iterdir()
               if entry.resolve().is_relative_to(ports[0])]
    require(len(targets) == 1, "Ambiguous ATA targets")
    words = (targets[0] / "id").read_text().split()
    require(len(words) == 256 and bytes.fromhex("".join(words[27:47])).decode("ascii").rstrip() == SSD,
            "Unexpected ATA IDENTIFY model")
    require(int((sysdisk / "size").read_text()) == SECTORS and
            int((sysdisk / "queue/logical_block_size").read_text()) == SECTOR and
            (sysdisk / "dev").read_text().strip() == device_number(disk), "Unexpected SSD geometry or identity")
    return disk


def partition_rows(table, plan, ready):
    root = plan["ubuntu_partition"]
    work = plan["scratch_partition"]
    values = [(*EFI, plan["efi_partuuid"]),
              (ROOT_START, ROOT_NEW if ready else ROOT_OLD, ROOT_TYPE, root["partuuid"])]
    if ready:
        values.append((WORK_START, WORK_SECTORS, WORK_TYPE, work["partuuid"]))
    require((table["label"], table["id"].lower(), table["unit"], table["sectorsize"]) ==
            ("gpt", plan["disk_guid"], "sectors", SECTOR), "GPT device identity differs")
    parts = table["partitions"]
    seen = [(part["start"], part["size"], part["type"].lower(), part["uuid"].lower()) for part in parts]
    require(seen == values, "GPT partitions, types, or identities differ")
    require(not ready or parts[2].get("name") == WORK_NAME, "Wrong work partition name")
    return parts


def layout(disk, plan, ready):
    table = json.loads(command("sfdisk", "--json", disk))["partitiontable"]
    parts = partition_rows(table, plan, ready)
    require([part["node"] for part in parts] == [f"{disk}{i + 1}" for i in range(len(parts))],
            "Partition device numbers differ")
    root = Path(parts[1]["node"])
    identity = command("blkid", "-p", "-o", "export", root).splitlines()
    require("TYPE=ext4" in identity and f"UUID={plan['ubuntu_partition']['filesystem_uuid']}" in identity,
            "Ubuntu filesystem UUID or type differs")
    return parts


def require_offline(disk, parts):
    require("boot=casper" in Path("/proc/cmdline").read_text().split() and
            Path("/cdrom/.disk/info").is_file(), "Boot Ubuntu live USB in Try Ubuntu mode")
    mounted = {fields[2] for line in Path("/proc/self/mountinfo").read_text().splitlines()
               if len(fields := line.split()) >= 3}
    swaps = {Path(line.split()[0]).resolve() for line in Path("/proc/swaps").read_text().splitlines()[1:]}
    for node in (disk, *(Path(part["node"]) for part in parts)):
        require(device_number(node) not in mounted and node not in swaps and
                not any((Path("/sys/class/block") / node.name / "holders").iterdir()),
                f"Device {node} is mounted, active as swap, or has holders")


def new_table(original, root_node, work_node, plan):
    lines = original.splitlines(keepends=True)
    targets = [i for i, line in enumerate(lines) if line.startswith(f"{root_node} ") and ":" in line]
    require(len(targets) == 1, "Expected one Ubuntu entry in the original sfdisk dump")
    i = targets[0]
    lines[i], count = re.subn(r"(?<=\bsize=)(\s*)" + str(ROOT_OLD) + r"(?=,)",
                             lambda match: match[1] + str(ROOT_NEW), lines[i])
    require(count == 1, "Could not replace the exact original Ubuntu size")
    lines.append(f'{work_node} : start={WORK_START}, size={WORK_SECTORS}, '
                 f'type={WORK_TYPE}, uuid={plan["scratch_partition"]["partuuid"]}, name="{WORK_NAME}"\n')
    return "".join(lines)


def fs_geometry(root):
    info = command("dumpe2fs", "-h", root)
    count = int(re.search(r"^Block count:\s+(\d+)$", info, re.MULTILINE)[1])
    size = int(re.search(r"^Block size:\s+(\d+)$", info, re.MULTILINE)[1])
    return count, size


def shrink(root):
    command("e2fsck", "-f", "-p", root, accepted=(0, 1))
    count, size = fs_geometry(root)
    limit = (ROOT_NEW * SECTOR - 32 * 1024 * 1024) // size
    if count > limit:
        command("resize2fs", root, str(limit))
    require(fs_geometry(root)[0] * size <= limit * size, "Filesystem still exceeds the new Ubuntu boundary")
    command("e2fsck", "-f", "-p", root, accepted=(0, 1))


def off_disk_backup(path, plan, disk):
    path = path.resolve(strict=True)
    require(path.is_dir(), "Choose an existing evidence USB directory")
    device = (Path("/dev/disk/by-uuid") / plan["evidence_uuid"]).resolve(strict=True)
    require((Path("/sys/block") / disk.name).resolve() not in
            (Path("/sys/class/block") / device.name).resolve().parents and
            command("findmnt", "-T", path, "-n", "-o", "MAJ:MIN").strip() == device_number(device),
            "Backup destination must reside on the configured evidence USB")
    target = path / "air61-layout-backup"
    target.mkdir()
    return target


def durable_text(path, content):
    with path.open("x") as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())


def prepare(plan, backup_dir=None):
    require(os.geteuid() == 0, "Run this helper with sudo in the live session")
    disk = disk_for_plan(plan)
    parts = layout(disk, plan, False)
    require_offline(disk, parts)
    command("sfdisk", "--verify", disk)
    root = Path(parts[1]["node"])
    original = command("sfdisk", "--dump", disk)
    proposed = new_table(original, root, Path(f"{disk}3"), plan)
    options = ("sfdisk", "--wipe", "never", "--wipe-partitions", "never", "--lock=yes")
    command(*options, "--no-act", disk, script=proposed)
    if backup_dir is None:
        return "PASS: live disk is offline and the 4 GiB layout passed the dry run"
    backup = off_disk_backup(backup_dir, plan, disk)
    durable_text(backup / "original.sfdisk", original)
    durable_text(backup / "proposed.sfdisk", proposed)
    # Archive raw GPT bytes as well as the text form before touching ext4.
    with disk.open("rb", buffering=0) as source:
        head = source.read(34 * SECTOR)
        source.seek((SECTORS - 33) * SECTOR)
        tail = source.read(33 * SECTOR)
    require(len(head) == 34 * SECTOR and len(tail) == 33 * SECTOR, "Could not back up both GPT copies")
    for name, data in (("primary.bin", head), ("backup.bin", tail)):
        with (backup / name).open("xb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
    os.sync()
    require_offline(disk, parts)
    shrink(root)
    # Revalidate the original table immediately before the sole GPT mutation.
    require_offline(disk, layout(disk, plan, False))
    require(command("sfdisk", "--dump", disk) == original, "GPT changed during ext4 shrink")
    command(*options, disk, script=proposed)
    command("udevadm", "settle")
    command("sfdisk", "--verify", disk)
    final = layout(disk, plan, True)
    require_offline(disk, final)
    require(final[0] == parts[0] and final[1] == {**parts[1], "size": ROOT_NEW},
            "An EFI or Ubuntu GPT field besides the intended size changed")
    command("e2fsck", "-f", "-p", root, accepted=(0, 1))
    durable_text(backup / "confirmed.sfdisk", command("sfdisk", "--dump", disk))
    os.sync()
    return "PASS: 4 GiB work partition created, Ubuntu UUID and ext4 verified"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("check", "apply"))
    parser.add_argument("plan", type=Path)
    parser.add_argument("--backup-dir", type=Path)
    args = parser.parse_args()
    if (args.action == "apply") != (args.backup_dir is not None):
        parser.error("apply requires --backup-dir on the verified evidence USB")
    print(prepare(read_plan(args.plan), args.backup_dir))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as exc:
        sys.exit(f"Air6,1 offline work partition: {exc}")
