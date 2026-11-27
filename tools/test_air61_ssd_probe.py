import hashlib
import struct
import subprocess
import tempfile
import unittest
import uuid
import zlib
from pathlib import Path
from unittest.mock import patch

import air61_ssd_probe as probe
import hardware_boot as boot
from mk_usb_image import gpt_entry, gpt_header

RELEASE = "0123456789abcdef"
VOLUME = "6ada85bb"


def fixture(overlap=False):
    mbr = bytearray(probe.SECTOR)
    mbr[450] = 0xee
    struct.pack_into("<II", mbr, 454, 1, probe.SECTORS - 1)
    mbr[510:512] = b"\x55\xaa"
    last = probe.SECTORS - 34
    entries = bytearray(128 * 128)
    entries[:128] = gpt_entry(uuid.UUID(bytes_le=probe.EFI), uuid.UUID(int=1), 2048, 8191, "ESP")
    entries[128:256] = gpt_entry(uuid.UUID(int=3), uuid.UUID(int=2),
                                 8191 if overlap else 8192, last, "Data")
    crc, guid = zlib.crc32(entries), uuid.UUID(int=4)
    primary = gpt_header(1, probe.SECTORS - 1, 34, last, 2, 128, crc, guid)
    backup = gpt_header(probe.SECTORS - 1, 1, 34, last, probe.SECTORS - 33, 128, crc, guid)
    return bytes(mbr) + primary + entries, bytes(entries) + backup, bytes(mbr)


def reset_trace(release, volume, ready=True):
    rows = [probe.probe_profile(release, volume, True, True),
            "air61-ssd-controller: firmware_ghc=0x0000000080000000 firmware_port=1",
            "air61-ssd-reset: result=ready attempts=1 hold_us=1000 wait_us=321" if ready else
            "air61-ssd-reset: result=timeout attempts=1 hold_us=1000 wait_us=2000000"]
    phases = ["firmware", "before-stop", "after-stop", "buffers-programmed", "fis-enabled",
              "reset-asserted", "reset-released", "reset-return"]
    if ready:
        phases.extend(("engine-started", "identify-return"))
    phases.append("retire-return")
    for index, phase in enumerate(phases):
        registers = dict.fromkeys("ghc cmd tfd sig ssts sctl serr is ci sact clb fb".split(), 0)
        registers.update(ghc=1 << 31, tfd=0x50 if ready and index >= 7 else 0x80,
                         sig=0x101, ssts=0x133, sctl=0x301 if phase == "reset-asserted" else 0x300,
                         fb=0x2000 if index >= 3 else 0x1000)
        values = " ".join(f"{key}=0x{value:016x}" for key, value in registers.items())
        d2h = 0x00504034 if ready and index >= 7 else 0
        rows.append(f"air61-ssd-port: phase={phase} port=0 us={index} {values} fis_valid={int(index >= 3)} "
                    f"pio=0x0000000000000000 d2h=0x{d2h:016x} sdb=0x0000000000000000")
    return rows


class Probe(unittest.TestCase):
    def test_direct_read_hashes_only_the_requested_slice(self):
        chunk = bytes(range(256)) * 32

        def preadv(fd, buffers, offset):
            self.assertEqual((fd, offset, len(buffers[0])), (7, 8192, len(chunk)))
            buffers[0][:] = chunk
            return len(chunk)

        with patch.object(probe.os, "preadv", side_effect=preadv, create=True):
            offset, length = 8192 + 7 * 512, 2 * 512
            self.assertEqual(probe._direct_read(7, length, offset), chunk[3584:4608])
        with patch.object(probe.os, "preadv", return_value=511, create=True):
            with self.assertRaisesRegex(ValueError, "Short direct"):
                probe._direct_read(7, 512, 0)

    def test_clean_efi_reference_restores_mount_and_keeps_complete_sector(self):
        disk, partition, target = Path("/dev/test"), Path("/dev/test1"), Path("/boot/efi")
        _, _, clean_sector = fixture()
        mounted_sector = bytearray(clean_sector)
        mounted_sector[65] = 1
        clean = {"samples": ["gpt1", "gpt2", "clean"]}
        mounted = {"samples": ["gpt1", "gpt2", "mounted"]}
        options = frozenset(("rw", "relatime"))
        for mode in ("passed", "read-error", "umount-error", "alias"):
            attached = True
            events = []

            def mounted_efi(actual, mount):
                self.assertEqual((actual, mount), (disk, target))
                self.assertTrue(attached)
                return partition, options

            def find(*args):
                if mode == "alias" and args[0] == "-S" and not attached:
                    return ["/alternate"]
                return ["mounted"] if attached else []

            def run(args, *, check, timeout):
                nonlocal attached
                self.assertTrue(check)
                self.assertEqual((args[1:], timeout), (("--", str(target)), 20))
                events.append(args[0])
                if mode == "umount-error":
                    raise subprocess.CalledProcessError(1, args)
                attached = args[0] == "mount"

            def capture(actual, *, direct, partition=False):
                self.assertEqual(actual, disk)
                self.assertTrue(direct)
                self.assertEqual(partition, not attached)
                events.append("mounted-read" if attached else "clean-read")
                if attached:
                    return mounted, bytes(mounted_sector)
                if mode == "read-error":
                    raise OSError("fixture read fault")
                return clean, clean_sector

            with (patch.object(probe, "_efi_mount", side_effect=mounted_efi),
                  patch.object(probe, "_find_efi", side_effect=find),
                  patch.object(probe, "_ubuntu_capture", side_effect=capture),
                  patch.object(probe.subprocess, "run", side_effect=run)):
                if mode == "passed":
                    self.assertEqual(probe.clean_ubuntu_reference(disk, partition=True),
                                     (clean, clean_sector, bytes(mounted_sector)))
                else:
                    with self.assertRaises((OSError, subprocess.CalledProcessError, ValueError)):
                        probe.clean_ubuntu_reference(disk, partition=True)
            expected = ["mounted-read", "umount"]
            if mode in ("passed", "read-error"):
                expected += ["clean-read", "mount"]
            self.assertEqual(events, expected)
            self.assertEqual(attached, mode != "alias")

    def test_reset_receipts(self):
        v2 = boot.limine_config(RELEASE, f"ram-usb-v2-air61-ssd-{VOLUME}")
        v3 = boot.limine_config(RELEASE, f"ram-usb-v3-air61-ssd-{VOLUME}")
        self.assertEqual(v3.replace(" srvros.ssd-reset=port0", ""), v2)
        good = "\n".join(reset_trace(RELEASE, VOLUME))
        timeout = "\n".join(reset_trace(RELEASE, VOLUME, ready=False))
        for receipt, status in ((good, "ready"), (timeout, "timeout")):
            result = probe.read_port_trace(receipt, RELEASE, VOLUME, reset=True)
            self.assertEqual(result["reset"]["status"], status)
        for bad in (good.replace("attempts=1", "attempts=2"),
                    good.replace("hold_us=1000", "hold_us=999"),
                    good.replace("sctl=0x0000000000000301", "sctl=0x0000000000000300"),
                    good.replace("d2h=0x0000000000504034", "d2h=0x0000000000000000"),
                    good.replace("tfd=0x0000000000000050", "tfd=0x0000000000000080")):
            with self.assertRaises(ValueError):
                probe.read_port_trace(bad, RELEASE, VOLUME, reset=True)

    def test_port_trace_accepts_complete_busy_probe(self):
        version = f"ram-usb-v2-air61-ssd-{VOLUME}"
        config = boot.limine_config(RELEASE, version)
        v1 = boot.limine_config(RELEASE, f"ram-usb-v1-air61-ssd-{VOLUME}")
        self.assertEqual(config.replace(" srvros.ssd-port-state=1", ""), v1)
        header = [probe.probe_profile(RELEASE, VOLUME, True),
                  "air61-ssd-controller: firmware_ghc=0x0000000080000000 firmware_port=1"]
        phases = ("firmware", "before-stop", "after-stop", "buffers-programmed",
                  "fis-enabled", "engine-started", "identify-return", "retire-return")
        rows = []
        registers = dict.fromkeys("ghc cmd tfd sig ssts sctl serr is ci sact clb fb".split(), 0)
        registers.update(ghc=1 << 31, tfd=0x80, sig=0x101, ssts=0x133)
        hexfields = " ".join(f"{key}=0x{value:016x}" for key, value in registers.items())
        for index, phase in enumerate(phases):
            rows.append(f"air61-ssd-port: phase={phase} port=0 us={index} {hexfields} fis_valid={int(index >= 3)} "
                        "pio=0x0000000000000000 d2h=0x0000000000000000 sdb=0x0000000000000000")
        failure = "air61-ssd: result=FAIL internal_mounted=no ata_writes=0"
        result = probe.read_port_trace("\n".join([*header, *rows, failure]), RELEASE, VOLUME)
        self.assertEqual(result["status"], "COMPLETE")
        self.assertEqual([row["tfd"] for row in result["snapshots"]], [0x80] * 8)
        with_ae_clear = [header[0], "air61-ssd-controller: firmware_ghc=0x0000000000000000 firmware_port=0"]
        self.assertEqual(len(probe.read_port_trace("\n".join([*with_ae_clear, *rows[1:]]),
                                                   RELEASE, VOLUME)["snapshots"]), 7)
        for bad in (rows[1:], rows[:-1], rows[::-1], rows + [rows[-1]],
                    [rows[0].replace("fis_valid=0", "fis_valid=1"), *rows[1:]],
                    [*rows[:-1], rows[-1].replace(" us=7", " us=1")]):
            with self.assertRaises(ValueError):
                probe.read_port_trace("\n".join([*header, *bad]), RELEASE, VOLUME)

    def test_gpt_and_native_receipt(self):
        with tempfile.TemporaryFile() as disk:
            disk.truncate(probe.SECTORS * probe.SECTOR)
            for (lba, _), data in zip(probe.SAMPLES, fixture()):
                disk.seek(lba * probe.SECTOR)
                disk.write(data)
            disk.flush()
            baseline = probe.read_samples(disk.fileno())
            self.assertEqual(probe._read_samples(disk.fileno()), (baseline, fixture()[2]))
        cmdline = next(line.strip().removeprefix("cmdline: ")
                       for line in boot.limine_config(RELEASE, f"ram-usb-v1-air61-ssd-{VOLUME}").splitlines()
                       if "cmdline:" in line)
        rows = [f"air61-ssd: profile {cmdline}", probe.IDENTITY]
        rows += ["air61-ssd-sample: lba={lba} sectors={sectors} sha256={sha256}".format(**sample)
                 for sample in baseline["samples"]]
        rows.append(probe.PASS)
        result = probe.compare_receipt("\n".join(rows), RELEASE, VOLUME, baseline)
        self.assertEqual((result["status"], result["partitions"]), ("PASS", 2))
        for bad in (rows[::-1], rows[:-1], rows + [rows[-1]], rows[:2] + [rows[3]] + rows[3:],
                    rows[:4] + [rows[4][:-1] + ("0" if rows[4][-1] != "0" else "1"), rows[-1]]):
            with self.assertRaises(ValueError):
                probe.compare_receipt("\n".join(bad), RELEASE, VOLUME, baseline)
        with self.assertRaisesRegex(ValueError, "completion"):
            probe.compare_receipt("\n".join(rows), "1" * 16, VOLUME, baseline)

    def test_refusals(self):
        primary, backup, _ = fixture()
        for bad_index in (510, 512 + 16, 1025):
            bad = bytearray(primary)
            bad[bad_index] ^= 1
            with self.assertRaises(ValueError):
                probe.verify_gpt(bad, backup)
        bad = bytearray(backup)
        bad[-probe.SECTOR + 16] ^= 1
        with self.assertRaisesRegex(ValueError, "CRC"):
            probe.verify_gpt(primary, bad)
        with self.assertRaises(ValueError):
            probe.verify_gpt(primary, backup[:-1])
        with self.assertRaisesRegex(ValueError, "Overlapping"):
            probe.verify_gpt(*fixture(overlap=True)[:2])

    def test_partition_profile_and_exact_receipt(self):
        v3 = boot.limine_config(RELEASE, f"ram-usb-v3-air61-ssd-{VOLUME}")
        v4 = boot.limine_config(RELEASE, f"ram-usb-v4-air61-ssd-{VOLUME}")
        flag = " srvros.ssd-partition-read=1"
        self.assertEqual(v4.replace(flag, ""), v3)
        with self.assertRaisesRegex(ValueError, "require the port 0 SATA reset"):
            boot.stage(air61_ssd_probe_volume=VOLUME, air61_ssd_partition_read=True)
        with tempfile.TemporaryFile() as disk:
            disk.truncate(probe.SECTORS * probe.SECTOR)
            for (lba, _), data in zip(probe.SAMPLES, fixture()):
                disk.seek(lba * probe.SECTOR)
                disk.write(data)
            disk.seek((2048 + 32) * probe.SECTOR)
            content = b"3" * (16 * probe.SECTOR)
            disk.write(content)
            disk.flush()
            baseline, _ = probe._read_samples(disk.fileno(), partition=True)
        self.assertEqual(baseline["partition_samples"][1],
                         dict(offset=32, sectors=16, commands=2, sha256=hashlib.sha256(content).hexdigest()))
        self.assertEqual(baseline["partition_samples"][-1]["offset"], 6136)

        trace = reset_trace(RELEASE, VOLUME)
        profile, *port = trace
        gpt = ["air61-ssd-gpt: lba={lba} sectors={sectors} efi={efi}".format(**p)
               for p in baseline["partition_layout"]]
        samples = ["air61-ssd-sample: lba={lba} sectors={sectors} sha256={sha256}".format(**s)
                   for s in baseline["samples"]]
        parts = ["air61-ssd-partition: offset={offset} sectors={sectors} commands={commands} sha256={sha256}".format(**s)
                 for s in baseline["partition_samples"]]
        rows = [profile + flag, probe.IDENTITY, *samples, *gpt, *parts, *port, probe.PASS]
        receipt = "\n".join(rows)
        result = probe.compare_receipt(receipt, RELEASE, VOLUME, baseline, reset=True, partition=True)
        self.assertEqual(result["partition_read_commands"], 4)
        mutations = (
            receipt.replace(gpt[0], gpt[0].replace("2048", "2049")),
            receipt.replace(parts[1], parts[1].replace("commands=2", "commands=16")),
            receipt.replace(parts[2], parts[0]),
            receipt.replace(parts[1], ""),
            receipt.replace(parts[1], parts[1].replace(hashlib.sha256(content).hexdigest(), "0" * 64)),
            receipt.replace(port[-1], port[-1].replace("cmd=0x0000000000000000", "cmd=0x0000000000000010")),
        )
        for bad in mutations:
            with self.assertRaises(ValueError):
                probe.compare_receipt(bad, RELEASE, VOLUME, baseline, reset=True, partition=True)

        discovery_flag = " srvros.ssd-discovery=normal"
        v5 = boot.limine_config(RELEASE, f"ram-usb-v5-air61-ssd-{VOLUME}")
        self.assertEqual(v5.replace(discovery_flag, ""), v4)
        with self.assertRaisesRegex(ValueError, "requires the partition-read profile"):
            boot.stage(air61_ssd_probe_volume=VOLUME, air61_ssd_discovery=True)
        discovery = "\n".join([rows[0] + discovery_flag, probe.DISCOVERY_PROOF, *rows[1:]])
        result = probe.compare_receipt(discovery, RELEASE, VOLUME, baseline,
                                       reset=True, partition=True, discovery=True)
        self.assertEqual(result["initializer"], "normal")
        for bad in (receipt, discovery.replace(probe.DISCOVERY_PROOF, ""),
                    discovery.replace("read_only=yes", "read_only=no"),
                    discovery + "\n" + probe.DISCOVERY_PROOF):
            with self.assertRaises(ValueError):
                probe.compare_receipt(bad, RELEASE, VOLUME, baseline, reset=True, partition=True, discovery=True)
