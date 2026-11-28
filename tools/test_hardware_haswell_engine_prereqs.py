from pathlib import Path
import unittest

import hardware_boot as boot
from hardware_haswell_engine_prereqs import TOKEN, engine_prereqs_result


class EnginePrerequisites(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = Path("build/tests/hsw-engine-prereqs-receipt.log").read_text()

    def test_awake_samples_and_clear_pending_never_supply_a_hold(self):
        for text, bus_master_enabled in ((self.text, True), (self.text.replace("cmd=7", "cmd=3"), False)):
            result = engine_prereqs_result(text)
            self.assertEqual(result["status"], "OBSERVED")
            self.assertEqual(result["frame_delta"], 1)
            self.assertEqual(result["smi_delta"], 1)
            self.assertEqual((result["writes"], result["ring_reads"], result["protected_reads"]), (0, 0, 0))
            self.assertEqual((result["awake_owner"], result["requester_hold"]), ("unavailable", "unverified"))
            for row in result["rows"]:
                self.assertTrue(row["kernel_ack"] and row["thread_c0_observed"])
                self.assertEqual(row["bus_master_enabled"], bus_master_enabled)
                self.assertFalse(row["advanced_features"]["transactions_pending"])

    def test_unverifiable_or_authority_claiming_receipts_fail_closed(self):
        for old,new in (("retired=2","retired=1"),("ack=1","ack=4294967295"),
                        ("protected_reads=0","protected_reads=1"),("ring_reads=0","ring_reads=1"),
                        ("requester_hold=unverified","requester_hold=verified"),("awake_owner=unavailable","awake_owner=held"),
                        ("completed=yes","completed=NO"),(TOKEN,TOKEN+" srvros.hsw-ownership=read-only")):
            with self.subTest(old=old):
                self.assertIn(old,self.text)
                self.assertIsNone(engine_prereqs_result(self.text.replace(old,new,1)))

    def test_isolated_profile_requires_automatic_return(self):
        profile = "ram-usb-v10-hsw-laptop-probe-6ada85bb"
        opts = boot.limine_config("0123456789abcdef",profile+"-auto-return").split()
        self.assertEqual(opts.count(TOKEN),1)
        with self.assertRaisesRegex(ValueError,"automatic return"):
            boot.limine_config("0123456789abcdef",profile)
        with self.assertRaisesRegex(ValueError,"isolated automatic return"):
            boot.stage(haswell_engine_prereqs=True,haswell_ownership_refusal=True)
