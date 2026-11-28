from pathlib import Path
import unittest

import hardware_boot as boot
from check_hsw_capture_tail import postamble_bound
from hardware_haswell_ownership import HOLD_TOKEN, TOKEN, ownership_result


class OwnershipReceipt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = Path("build/tests/hsw-ownership-receipt.log").read_text()
        cls.hold = Path("build/tests/hsw-requester-hold-receipt.log").read_text()

    def test_verified_refusal_binds_raw_entries_to_live_image(self):
        result = ownership_result(self.text)
        self.assertEqual(result["status"], "REFUSAL_VERIFIED")
        self.assertEqual((result["first_slot"], result["count"], result["image_aliases"]), (24576, 16, 15))
        self.assertEqual(result["admission"], "refused")
        self.assertFalse(result["hardware_authority"])
        self.assertEqual((result["frame_delta"], result["writes"]), (1, 0))
        self.assertEqual(result["words"][-1], 0x8ea00001)
        self.assertTrue(all(p["reads"] == 16 and p["retired"] == 1 for p in result["passes"]))

    def test_claimed_authority_and_corrupt_observations_fail_closed(self):
        for old, new in (("admission=refused", "admission=granted"), ("range_grant=unavailable", "range_grant=held"),
                         ("firmware_exclusion=unverified", "firmware_exclusion=verified"), ("image_aliases=15", "image_aliases=16"),
                         ("first=24576", "first=4096"), ("first=24576", "first=16385"), ("retired=1", "retired=0"),
                         ("hash=", "hash=9"), ("completed=yes", "completed=NO"),
                         (TOKEN, TOKEN+" srvros.hsw-sweep=read-only")):
            with self.subTest(old=old):
                self.assertIn(old, self.text)
                self.assertIsNone(ownership_result(self.text.replace(old, new, 1)))

    def test_receipt_survives_the_log_tail(self):
        text = "x"*131072+"\n"+self.text+"y"*postamble_bound()+"\n"
        self.assertEqual(ownership_result(text[-65536:]), ownership_result(self.text))

    def test_isolated_profile_requires_automatic_return(self):
        profile = "ram-usb-v9-hsw-laptop-probe-6ada85bb"
        opts = boot.limine_config("0123456789abcdef", profile+"-auto-return").split()
        self.assertEqual(opts.count(TOKEN), 1)
        self.assertNotIn("srvros.hsw-sweep=read-only", opts)
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config("0123456789abcdef", profile)
        with self.assertRaisesRegex(ValueError, "isolated automatic return"):
            boot.stage(haswell_ownership_refusal=True, haswell_sweep=True)

    def test_hold_brackets_both_read_only_passes(self):
        result = ownership_result(self.hold)
        hold = result["software_hold"]
        first, last = hold["refusals"]
        self.assertEqual(result["status"], "REFUSAL_VERIFIED")
        self.assertFalse(result["hardware_authority"])
        self.assertEqual((hold["state"], hold["firmware_handoff"], hold["hardware_hold"]),
                         ("retired", "unavailable", "unverified"))
        self.assertLessEqual(hold["begin_us"], first)
        self.assertLessEqual(first, result["passes"][0]["begin"])
        self.assertLessEqual(result["passes"][1]["end"], last)
        self.assertEqual(last, hold["end_us"])
        self.assertEqual((hold["protected_reads"], result["writes"]), (0, 0))
        self.assertEqual(ownership_result(self.hold[:-len("laptop-capture: completed=yes\n")]), None)

    def test_hold_rejects_forged_authority_or_missing_refusal(self):
        for old, new in (("firmware_handoff=unavailable", "firmware_handoff=verified"),
                         ("hardware_hold=unverified", "hardware_hold=verified"),
                         ("state=retired", "state=retained"), ("refusals=2", "refusals=1"),
                         ("denied=", "denied=9"), ("protected_reads=0", "protected_reads=1"),
                         ("hsw-requester-hold:", "unknown:"), (HOLD_TOKEN, TOKEN),
                         (HOLD_TOKEN, HOLD_TOKEN + " srvros.hsw-forcewake=handoff-audit")):
            with self.subTest(old=old):
                self.assertIn(old, self.hold)
                self.assertIsNone(ownership_result(self.hold.replace(old, new, 1)))

    def test_hold_profile_is_exclusive_and_requires_automatic_return(self):
        profile = "ram-usb-v15-hsw-laptop-probe-6ada85bb"
        opts = boot.limine_config("0123456789abcdef", profile+"-auto-return").split()
        self.assertEqual(opts.count(HOLD_TOKEN), 1)
        self.assertNotIn(TOKEN, opts)
        self.assertFalse(any(opt.startswith("srvros.hsw-forcewake=") for opt in opts))
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config("0123456789abcdef", profile)
        with self.assertRaisesRegex(ValueError, "isolated automatic return"):
            boot.stage(haswell_requester_hold=True, haswell_handoff_audit=True, haswell_linux_fences=True)
