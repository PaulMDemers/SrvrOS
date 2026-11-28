from pathlib import Path
import re
import unittest

import hardware_boot as boot
from check_hsw_capture_tail import postamble_bound
from hardware_haswell_survey import FIRST_SLOTS, TOKEN, survey_result


class SurveyReceipt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = Path("build/tests/hsw-survey-receipt.log").read_text()

    def test_checked_shortlist_excludes_occupied_and_protected_slots(self):
        result = survey_result(self.text)
        self.assertEqual(result["status"], "OBSERVED")
        self.assertEqual(result["frame_delta"], 1)
        self.assertEqual(result["zero_slot_shortlist"], [8192, 24577])
        self.assertFalse(result["hardware_authority"])
        self.assertEqual([r["first_slot"] for r in result["ranges"]], list(FIRST_SLOTS))
        self.assertEqual(sum(r["nonzero_not_present"] for r in result["ranges"]), 1)
        self.assertEqual(sum(r["primary_image_aliases"] for r in result["ranges"]), 4092)
        for r in result["ranges"]:
            self.assertFalse(any(r["first_slot"] <= slot <= r["last_slot"] for slot in (4096, 16385)))
        self.assertTrue(all(p["retired"] == 4 for p in result["passes"]))

    def test_corruption_and_incomplete_success_fail_closed(self):
        for old, new in (("first=8192", "first=4096"), ("first=24576", "first=16385"),
                         ("retired=4", "retired=3"), ("writes=0", "writes=1"),
                         ("completed=yes", "completed=NO"), (TOKEN, TOKEN + " srvros.hsw-candidate=read-only")):
            with self.subTest(old=old):
                self.assertIsNone(survey_result(self.text.replace(old, new, 1)))
        self.assertIsNone(survey_result(re.sub(r"hash=\d+", "hash=0", self.text, count=1)))
        self.assertIsNone(survey_result(re.sub(r"(hsw-survey-data: first=8192 data=).", r"\g<1>B", self.text)))

    def test_complete_receipt_survives_bounded_postamble(self):
        text = "x"*131072 + "\n" + self.text + "y"*postamble_bound() + "\n"
        self.assertEqual(survey_result(text[-65536:]), survey_result(self.text))

    def test_isolated_profile_requires_automatic_return(self):
        profile = "ram-usb-v7-hsw-laptop-probe-6ada85bb"
        options = boot.limine_config("0123456789abcdef", profile+"-auto-return").split()
        self.assertEqual(options.count(TOKEN), 1)
        self.assertNotIn("srvros.hsw-candidate=read-only", options)
        self.assertNotIn(TOKEN, boot.limine_config("0123456789abcdef", profile.replace("v7", "v6")+"-auto-return"))
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config("0123456789abcdef", profile)
        with self.assertRaisesRegex(ValueError, "isolated automatic return"):
            boot.stage(haswell_survey=True, haswell_boot_slot=True)
