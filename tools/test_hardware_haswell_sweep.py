from pathlib import Path
import unittest

import hardware_boot as boot
from check_hsw_capture_tail import postamble_bound
from hardware_haswell_sweep import PROTECTED, TOKEN, sweep_result


class SweepReceipt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = Path("build/tests/hsw-sweep-receipt.log").read_text()

    def test_complete_coverage_and_protected_neighbors(self):
        result = sweep_result(self.text)
        self.assertEqual((result["status"], result["batches_completed"], result["entries"]), ("OBSERVED", 14, 57342))
        self.assertEqual(result["zero_slot_shortlist"], [4097, 16386, 65535])
        self.assertFalse(result["hardware_authority"])
        for row in result["batches"]:
            self.assertTrue(all(p["maps"] == p["retired"] == 4 for p in row["passes"]))
            self.assertFalse(PROTECTED.intersection(row["zero_slot_shortlist"]))
        self.assertEqual([b["index"] for b in result["batches"] if b["expected_reads_per_pass"] == 4095], [0, 2])

    def test_corrupt_complete_receipt_refused(self):
        corruptions = (("slot=4097", "slot=4096"), ("slot=16386", "slot=16385"),
                       ("pages=4096,", "pages=3072,"), ("retired=4", "retired=3"),
                       ("reads=4095", "reads=4096"), ("complete=14", "complete=13"),
                       ("hash=", "hash=9"), ("completed=yes", "completed=NO"))
        for old, new in corruptions:
            with self.subTest(old=old):
                self.assertIn(old, self.text)
                self.assertIsNone(sweep_result(self.text.replace(old, new, 1)))

    def test_partial_failure_never_publishes_a_full_shortlist(self):
        result = sweep_result(Path("build/tests/hsw-sweep-failed.log").read_text())
        self.assertEqual(result["status"], "FAILED")
        self.assertEqual((len(result["batches"]), result["batches_completed"]), (2, 1))
        self.assertNotIn("zero_slot_shortlist", result)
        self.assertNotIn("zero_slot_shortlist", result["batches"][1])

    def test_maximum_receipt_keeps_every_batch_in_the_log_tail(self):
        text = Path("build/tests/hsw-sweep-max.log").read_text()
        result = sweep_result(text)
        self.assertEqual(result["zero_count"], 57342)
        self.assertTrue(all(len(b["zero_slot_shortlist"]) == 16 for b in result["batches"]))
        self.assertEqual(sweep_result(("x"*131072+"\n"+text+"y"*postamble_bound()+"\n")[-65536:]), result)

    def test_isolated_profile_requires_automatic_return(self):
        profile = "ram-usb-v8-hsw-laptop-probe-6ada85bb"
        opts = boot.limine_config("0123456789abcdef", profile+"-auto-return").split()
        self.assertEqual(opts.count(TOKEN), 1)
        self.assertNotIn("srvros.hsw-survey=read-only", opts)
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config("0123456789abcdef", profile)
        with self.assertRaisesRegex(ValueError, "isolated automatic return"):
            boot.stage(haswell_sweep=True, haswell_survey=True)
