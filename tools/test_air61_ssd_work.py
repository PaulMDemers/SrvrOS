import json
from pathlib import Path
import shutil
import struct
import tempfile
import unittest
from unittest.mock import patch
import uuid
import zlib

import air61_ssd_scratch as scratch
import air61_ssd_work as trial
import air61_work_layout as work
import hardware_boot as boot
from mk_usb_image import gpt_entry, gpt_header
from test_air61_ssd_probe import reset_trace


def fixture_plan():
    def identity(value):
        return str(uuid.UUID(int=value))

    return {
        "model": work.MODEL, "model_from_linux": work.SSD,
        "logical_sector_bytes": work.SECTOR, "sectors": work.SECTORS,
        "disk_guid": identity(1), "efi_partuuid": identity(2), "evidence_uuid": "6ADA-85BB",
        "ubuntu_partition": dict(partuuid=identity(3), filesystem_uuid=identity(4),
                                 first_sector=work.ROOT_START, current_sectors=work.ROOT_OLD,
                                 new_sectors=work.ROOT_NEW, new_last_sector=work.WORK_START - 1),
        "scratch_partition": dict(partuuid=identity(5), name=work.WORK_NAME, first_sector=work.WORK_START,
                                  last_sector=work.WORK_START + work.WORK_SECTORS - 1,
                                  sectors=work.WORK_SECTORS, type=work.WORK_TYPE),
    }


def original_gpt(plan, ready=False, root_delta=0):
    efi_first, efi_size, efi_type = work.EFI
    root_size = (work.ROOT_NEW if ready else work.ROOT_OLD) + root_delta
    entries = gpt_entry(uuid.UUID(efi_type), uuid.UUID(plan["efi_partuuid"]),
                        efi_first, efi_first + efi_size - 1, "EFI System Partition")
    entries += gpt_entry(uuid.UUID(work.ROOT_TYPE), uuid.UUID(plan["ubuntu_partition"]["partuuid"]),
                         work.ROOT_START, work.ROOT_START + root_size - 1, "Ubuntu")
    if ready:
        entries += gpt_entry(uuid.UUID(work.WORK_TYPE), uuid.UUID(plan["scratch_partition"]["partuuid"]),
                             work.WORK_START, work.WORK_START + work.WORK_SECTORS - 1, work.WORK_NAME)
    entries = entries.ljust(128 * 128, b"\0")
    mbr = bytearray(work.SECTOR)
    final_lba = work.SECTORS - 1
    mbr[446:462] = struct.pack("<B3sB3sII", 0, b"\x00\x02\x00", 0xee, b"\xff\xff\xff", 1, final_lba)
    mbr[510:] = b"\x55\xaa"
    crc = zlib.crc32(entries)
    disk = uuid.UUID(plan["disk_guid"])
    return (bytes(mbr) + gpt_header(1, final_lba, 34, final_lba - 33, 2, 128, crc, disk) + entries,
            entries + gpt_header(final_lba, 1, 34, final_lba - 33, final_lba - 32, 128, crc, disk))


class WorkPartitionTest(unittest.TestCase):
    def setUp(self):
        self.plan = fixture_plan()

    def test_preflight_refuses_wrong_gpt_before_fixture_is_usable(self):
        primary, backup = original_gpt(self.plan, ready=True)
        self.assertEqual(trial.verified_gpt(primary, backup, self.plan), scratch.sha(primary) + scratch.sha(backup))
        spare = scratch.gpt(*(uuid.UUID(int=n) for n in (1, 2, 5)))
        for pair in (spare, original_gpt(self.plan), original_gpt(self.plan, ready=True, root_delta=-1),
                     (primary, bytes([backup[0] ^ 1]) + backup[1:])):
            with self.assertRaises(ValueError):
                trial.verified_gpt(*pair, self.plan)
        self.plan["scratch_partition"]["partuuid"] = str(uuid.UUID(int=6))
        with self.assertRaisesRegex(ValueError, "partition 3"):
            trial.verified_gpt(primary, backup, self.plan)

    @unittest.skipUnless(shutil.which("sfdisk"), "requires local util-linux")
    def test_offline_exact_table_change_on_regular_sparse_image(self):
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / "test.img"
            primary, backup = original_gpt(self.plan)
            with image.open("xb") as stream:
                stream.truncate(work.SECTORS * work.SECTOR)
                stream.write(primary)
                stream.seek((work.SECTORS - 33) * work.SECTOR)
                stream.write(backup)
            before = work.command("sfdisk", "--dump", image)
            current = json.loads(work.command("sfdisk", "--json", image))["partitiontable"]
            efi, root = work.partition_rows(current, self.plan, False)
            planned = work.new_table(before, root["node"], image.with_name(image.name + "3"), self.plan)
            options = ("sfdisk", "--wipe", "never", "--wipe-partitions", "never", "--lock=yes")
            work.command(*options, "--no-act", image, script=planned)
            self.assertEqual(before, work.command("sfdisk", "--dump", image))
            work.command(*options, image, script=planned)
            work.command("sfdisk", "--verify", image)
            final = json.loads(work.command("sfdisk", "--json", image))["partitiontable"]
            self.assertEqual(work.partition_rows(final, self.plan, True)[:2],
                             [efi, {**root, "size": work.ROOT_NEW}])
            with image.open("rb") as stream:
                new_primary = stream.read(len(primary))
                stream.seek((work.SECTORS - 33) * work.SECTOR)
                new_backup = stream.read(len(backup))
            trial.verified_gpt(new_primary, new_backup, self.plan)

    @unittest.skipUnless(all(shutil.which(tool) for tool in
                            ("mkfs.ext4", "dumpe2fs", "e2fsck", "resize2fs", "debugfs")), "requires e2fsprogs")
    def test_offline_ext4_shrink_preserves_uuid_and_data(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            image = folder / "ext4.img"
            contents = folder / "files"
            contents.mkdir()
            marker = "Air6,1 ext4 shrink rehearsal\n"
            (contents / "marker.txt").write_text(marker)
            with image.open("xb") as stream:
                stream.truncate(96 * 1024 * 1024)
            identity = self.plan["ubuntu_partition"]["filesystem_uuid"]
            work.command("mkfs.ext4", "-q", "-U", identity, "-d", contents, image)
            with patch.object(work, "ROOT_NEW", 92 * 1024 * 1024 // work.SECTOR):
                work.shrink(image)
            self.assertEqual(work.command("blkid", "-p", "-s", "UUID", "-o", "value", image).strip(), identity)
            self.assertEqual(work.command("debugfs", "-R", "cat /marker.txt", image), marker)
            blocks, size = work.fs_geometry(image)
            self.assertEqual(blocks * size, 60 * 1024 * 1024)

    def test_exact_boot_profile_and_receipt_use_internal_extent(self):
        proof = trial.verified_gpt(*original_gpt(self.plan, ready=True), self.plan)
        version = f"ram-usb-v1-air61-work-6ada85bb-{proof}"
        cmdline = scratch.profile("6ada85bb", proof, internal=True)
        config = boot.limine_config("deadbeefdeadbeef", version)
        self.assertIn(f"cmdline: {cmdline}\n", config)
        self.assertEqual(config.count("cmdline:"), 1)
        write_hash = scratch.sha(scratch.scratch_pages(proof, written=True))
        expected = dict(workspace_volume="6ada85bb", fixture=proof,
                        written_pages_sha256=write_hash, write_lba=trial.WRITE_LBA)
        log = "\n".join([
            f"air61-scratch: profile {cmdline}", "air61-scratch: phase=verify",
            f"air61-scratch: pages-sha256={write_hash} lba={trial.WRITE_LBA} sectors=8 writes=0 flushes=0",
            *reset_trace("unused", "6ada85bb")[1:], "air61-scratch: result=PASS internal_mounted=no",
        ])
        self.assertEqual(scratch.compare_log(log, expected, "verify", internal=True)["status"], "ready")
        for bad in (log.replace("writes=0", "writes=1"),
                    log.replace(f"lba={trial.WRITE_LBA}", f"lba={work.WORK_START - 1}")):
            with self.assertRaises(ValueError):
                scratch.compare_log(bad, expected, "verify", internal=True)
        with self.assertRaises(ValueError):
            scratch.compare_log(log, expected, "verify")
