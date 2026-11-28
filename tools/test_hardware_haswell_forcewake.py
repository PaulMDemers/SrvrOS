import unittest
from pathlib import Path

import hardware_boot as boot
from hardware_haswell_forcewake import TOKEN, forcewake_result


class HaswellForcewakeReceipt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = Path("build/tests/hsw-forcewake-receipt.log").read_text()

    def test_live_receipt_requires_clear_set_c0_clear_and_frame_progress(self):
        result = forcewake_result(self.text)
        self.assertEqual(result["status"], "VERIFIED")
        self.assertEqual((result["stores"], result["retired"], result["frame_delta"]), (2, 2, 1))
        self.assertEqual(result["requester_hold"], "unverified")
        for old, new in (
            ("baseline=0", "baseline=1"),
            ("acquired=1", "acquired=0"),
            ("thread=0", "thread=3"),
            ("held=1", "held=0"),
            ("final=0", "final=1"),
            ("released=1", "released=0"),
            ("retired=2", "retired=1"),
            ("frames=4294967295,0", "frames=4294967295,4294967295"),
            ("ring_reads=0", "ring_reads=1"),
            ("protected_reads=0", "protected_reads=1"),
            ("laptop-capture: completed=yes", "laptop-capture: completed=NO"),
            (TOKEN, TOKEN + " srvros.hsw-candidate=read-only"),
        ):
            with self.subTest(new=new):
                self.assertIn(old, self.text)
                self.assertIsNone(forcewake_result(self.text.replace(old, new, 1)))

    def test_retained_failure_cannot_claim_release_or_retirement(self):
        failure = (self.text.replace("status=1 reason=none", "status=5 reason=release-unverified")
                   .replace("retired=2", "retired=0").replace("released=1", "released=0")
                   .replace("final=0", "final=1").replace("completed=yes", "completed=NO"))
        self.assertEqual(forcewake_result(failure)["status"], "FAILED")
        self.assertIsNone(forcewake_result(failure.replace("retired=0", "retired=1")))
        self.assertIsNone(forcewake_result(failure.replace("released=0", "released=1")))

    def test_stage_recipe_is_exclusive_and_automatic_return(self):
        version = "ram-usb-v11-hsw-laptop-probe-6ada85bb"
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config("1234567890abcdef", version)
        config = boot.limine_config("1234567890abcdef", version + "-auto-return")
        self.assertIn(TOKEN, config)
        self.assertNotIn("srvros.hsw-engine-prereqs=", config)
        with self.assertRaisesRegex(ValueError, "isolated automatic return"):
            boot.stage(haswell_forcewake=True, haswell_ownership_refusal=True)
        with self.assertRaisesRegex(ValueError, "isolated automatic return"):
            boot.stage(haswell_forcewake=True, haswell_linux_fences=True, hold_on_failure=True)


if __name__ == "__main__":
    unittest.main()
