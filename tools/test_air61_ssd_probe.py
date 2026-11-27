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
