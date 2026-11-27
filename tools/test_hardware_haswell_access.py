import base64
import hashlib
from pathlib import Path
import struct
import unittest
from unittest.mock import patch

import hardware_boot as boot
from hardware_haswell_access import access_result, _rmrrs
from test_hardware_haswell_firmware import fixture


def firmware_receipt():
    log, _ = fixture(count=42)
    first = next(line for line in log.splitlines() if line.startswith("hsw-firmware-dmar:"))
    old = first.split("data=", 1)[1]
    raw = bytearray(base64.b64decode(old))
    # One 32-byte RMRR with the direct IGD endpoint, then one opaque
    # structure filling the remaining synthetic table.
    raw[48:80] = struct.pack("<HHHHQQ8B", 1, 32, 0, 0, 0x8D800000, 0x8F9FFFFF,
                              1, 8, 0, 0, 0, 0, 2, 0)
    raw[80:84] = struct.pack("<HH", 0xffff, 56)
    raw[9] = 0
    raw[9] = -sum(raw) % 256
    return (log.replace(old, base64.b64encode(raw).decode()).split("laptop-capture:")[0],
            hashlib.sha256(raw).hexdigest(), raw)


class AccessReceipt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.log = Path("build/tests/hsw-access-receipt.log").read_text()

    def test_binds_observed_slot_to_rmrr_and_fbc_without_inventing_ownership(self):
        firmware, pin, _ = firmware_receipt()
        self.assertIsNone(access_result(self.log))
        self.assertIsNone(access_result(firmware + self.log))
        with patch("hardware_haswell_firmware.PIN", pin):
            result = access_result(firmware + self.log)
        self.assertEqual(result["status"], "OBSERVED")
        self.assertEqual(result["target"], 0x8EA00000)
        self.assertTrue(result["prior_entry_matches"])
        self.assertEqual(result["neighboring_aliases"], [4097])
        self.assertEqual(result["inherited_image_aliases"], [])
        self.assertEqual(result["fbc"], dict(enabled=True, buffer_base=0x8ea00000, relation="buffer_start"))
        self.assertEqual(result["declared_rmrrs_covering_target"][0]["scopes"][0]["path"], [(2, 0)])
        self.assertFalse(result["hardware_authority"])
        self.assertIn("live VT-d translation", result["unresolved"])

    def test_receipts_reject_drift_partial_and_out_of_contract_claims(self):
        firmware, pin, _ = firmware_receipt()
        text = firmware + self.log
        with patch("hardware_haswell_firmware.PIN", pin):
            for before, after in (("fbc_retired=2", "fbc_retired=1"), ("entry_rows=2", "entry_rows=1"),
                                  ("writes=0 authority=none", "writes=0 authority=write")):
                self.assertIsNone(access_result(text.replace(before, after)))
            for kind in ("entries", "fbc"):
                line = next(line for line in text.splitlines() if line.startswith(f"hsw-access-{kind}: phase=1"))
                record = line.split("data=", 1)[1]
                content = bytearray(base64.b64decode(record))
                content[0] ^= 1
                modified = base64.b64encode(content).decode()
                self.assertIsNone(access_result(text.replace(line, line.replace(record, modified))))
                self.assertIsNone(access_result(text.replace(line + "\n", "")))
                self.assertIsNone(access_result(text + line + "\n"))

    def test_rejects_malformed_rmrr_scope_and_range(self):
        _receipt, _pin, raw = firmware_receipt()
        for index, value in ((50, 4), (73, 7), (56, 1), (64, 0)):
            bad = raw.copy()
            bad[index] = value
            with self.assertRaises(ValueError):
                _rmrrs(bad)

    def test_recipe_is_distinct_and_cannot_skip_original_candidate_or_reboot(self):
        profile = "ram-usb-v5-hsw-laptop-probe-6ada85bb"
        opts = boot.limine_config("0123456789abcdef", profile + "-auto-return").split()
        self.assertIn("srvros.hsw-access=read-only", opts)
        self.assertIn("srvros.hsw-candidate=read-only", opts)
        self.assertNotIn("srvros.hsw-access=read-only", boot.limine_config(
            "0123456789abcdef", profile.replace("v5", "v4") + "-auto-return"))
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config("0123456789abcdef", profile)
        with self.assertRaisesRegex(ValueError, "requires the candidate profile"):
            boot.stage(haswell_access=True)
