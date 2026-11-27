import struct
import tempfile
import unittest
import uuid
import zlib

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


class Probe(unittest.TestCase):
    def test_gpt_and_native_receipt(self):
        with tempfile.TemporaryFile() as disk:
            disk.truncate(probe.SECTORS * probe.SECTOR)
            for (lba, _), data in zip(probe.SAMPLES, fixture()):
                disk.seek(lba * probe.SECTOR)
                disk.write(data)
            disk.flush()
            baseline = probe.read_samples(disk.fileno())
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
