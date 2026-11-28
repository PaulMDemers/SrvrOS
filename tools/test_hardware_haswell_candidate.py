import base64
from pathlib import Path
import struct
import unittest

import hardware_boot as boot
from hardware_haswell_candidate import candidate_result
from hardware_haswell_backing import canonical


class CandidateReceipt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.log = Path("build/tests/hsw-candidate-receipt.log").read_text()

    def test_fixture_replays_stable_slot_and_live_counter_wrap(self):
        result = candidate_result(self.log)
        self.assertEqual(result["status"], "OBSERVED")
        self.assertEqual(result["frame_delta"], 1)
        self.assertTrue(result["retired"])
        self.assertFalse(result["hardware_authority"])
        self.assertTrue(result["requesters"][0]["igd"]["transactions_pending"])
        self.assertIsNone(result["requesters"][0]["hda"]["transactions_pending"])

    def test_rejects_inconsistent_success(self):
        row = next(line for line in self.log.splitlines()
                   if line.startswith("hsw-candidate-row: phase=1"))
        payload = row.split("data=", 1)[1]
        values = list(struct.unpack("<25I", canonical(payload, 100)))
        for index, change in ((0, 0xffffffff), (1, 0x1001), (2, 0), (24, 0)):
            altered = values.copy()
            altered[index] = change
            replacement = base64.b64encode(struct.pack("<25I", *altered)).decode()
            self.assertIsNone(candidate_result(self.log.replace(row, row.replace(payload, replacement))))
        for old, new in (("mapped=1 retired=1", "mapped=1 retired=0"),
                         ("authority=none", "authority=write"), ("slot=4096", "slot=4097"),
                         ("completed=yes", "completed=NO")):
            self.assertIsNone(candidate_result(self.log.replace(old, new)))
        self.assertIsNone(candidate_result(self.log + row + "\n"))

    def test_profile_is_explicit_and_auto_return(self):
        profile = "ram-usb-v4-hsw-laptop-probe-6ada85bb"
        opts = boot.limine_config("0123456789abcdef", profile + "-auto-return").split()
        self.assertIn("srvros.hsw-candidate=read-only", opts)
        self.assertIn("srvros.hsw-fences=linux-model-32", opts)
        self.assertIn("srvros.capture-failure=reboot", opts)
        self.assertNotIn("srvros.hsw-candidate=read-only", boot.limine_config(
            "0123456789abcdef", profile.replace("v4", "v3") + "-auto-return"))
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config("0123456789abcdef", profile)

    def test_disjoint_slot_has_separate_profile_and_receipt(self):
        profile = "ram-usb-v6-hsw-laptop-probe-6ada85bb"
        token = "srvros.hsw-boot-slot=read-only"
        opts = boot.limine_config("0123456789abcdef", profile + "-auto-return").split()
        self.assertEqual(opts.count(token), 1)
        self.assertIn("srvros.hsw-candidate=read-only", opts)
        self.assertNotIn("srvros.hsw-access=read-only", opts)
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config("0123456789abcdef", profile)
        with self.assertRaisesRegex(ValueError, "isolated candidate"):
            boot.stage(haswell_boot_slot=True)
        text = Path("build/tests/hsw-boot-slot-receipt.log").read_text()
        proof = candidate_result(text, boot_slot=True)
        self.assertEqual((proof["slot"], proof["mmio_offset"], proof["pte"]), (16385, 0x210004, 0))
        self.assertTrue(proof["retired"])
        self.assertFalse(proof["hardware_authority"])
        self.assertIsNone(candidate_result(text))
        self.assertIsNone(candidate_result(self.log, boot_slot=True))
        for old, new in ((token, ""), (token, token + " srvros.hsw-access=read-only"),
                         ("0x210004", "0x210000")):
            self.assertIsNone(candidate_result(text.replace(old, new), boot_slot=True))
