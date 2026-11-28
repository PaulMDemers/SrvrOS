"""Check receipt identity, integrity and graphics handle relationships."""

import unittest
import zlib

import hardware_haswell_efi as efi


RELEASE = "0123456789abcdef"
NONCE = "0123456789abcdef" * 2
PRIVATE = "a4f10000-0000-0000-0000-000000000005"
BODY = f"""igd a0000000
gop 2 a0100000 420000 1408 1
protocol 1 {efi.PCI_IO}
open {efi.PCI_IO} 3 2 8 1
open {efi.PCI_IO} 3 1 10 1
protocol 1 {PRIVATE}
protocol 2 {efi.GOP}
protocol 3 {PRIVATE}
image 3 {PRIVATE}
complete 3 1
"""


def packet(body):
    data = body.encode()
    footer = f"end {zlib.crc32(data):08x}\n".encode()
    return (efi.seed(RELEASE, NONCE).rstrip(b"\0") + data + footer).ljust(efi.CAPACITY, b"\0")


class FirmwareInventoryTests(unittest.TestCase):
    def test_observation_never_supplies_firmware_authority(self):
        result = efi.decode(packet(BODY), RELEASE, NONCE)
        self.assertEqual(result["status"], "observed")
        self.assertEqual(result["firmware_exclusion"], "unverified")
        self.assertEqual(result["protocols"][1], [efi.PCI_IO, PRIVATE])
        self.assertEqual(result["modes"][2]["base"], 0xa0100000)
        self.assertEqual(result["opens"][0]["agent"], 3)
        self.assertEqual(result["images"][3], PRIVATE)

    def test_absence_and_capacity_refusal(self):
        for body, status in (("absent\n", "absent"), ("unavailable WriteFailed\n", "unavailable")):
            result = efi.decode(packet(body), RELEASE, NONCE)
            self.assertEqual(result["status"], status)
            self.assertEqual(result["firmware_exclusion"], "unverified")

    def test_refuses_stale_truncated_and_partial_receipts(self):
        with self.assertRaises(ValueError):
            efi.decode(packet(BODY), RELEASE, "0" * 32)
        for data in (efi.seed(RELEASE, NONCE), packet(BODY)[:-1],
                     packet(BODY).replace(b"a0100000", b"a0200000"),
                     packet(BODY + "authority verified\n")):
            with self.subTest(data=data[:100]), self.assertRaises(ValueError):
                efi.decode(data, RELEASE, NONCE)

    def test_rejects_inconsistent_driver_or_gop_relations(self):
        for bad in (BODY.replace("3 2 8 1", "4 2 8 1"),
                    BODY.replace("3 2 8 1", "3 2 1 1"),
                    BODY.replace("3 2 8 1", "3 2 8 0"),
                    BODY.replace("a0100000", "b0100000"),
                    BODY.replace("complete 3 1", "complete 3 2"),
                    BODY.replace(f"protocol 2 {efi.GOP}", f"protocol 2 {PRIVATE}"),
                    BODY.replace(f"protocol 1 {PRIVATE}", f"protocol 1 {efi.PCI_IO}")):
            with self.subTest(body=bad), self.assertRaises(ValueError):
                efi.decode(packet(bad), RELEASE, NONCE)
