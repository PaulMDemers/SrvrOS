import re
import unittest
from pathlib import Path

import hardware_boot as boot
from hardware_haswell_engine_drain import ENGINE_DRAIN_TOKEN, EXCLUSION_AUDIT_TOKEN, HANDOFF_AUDIT_TOKEN
from hardware_haswell_forcewake import forcewake_result


class HaswellEngineDrainReceipt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = Path("build/tests/hsw-engine-drain-receipt.log").read_text()
        cls.audit = Path("build/tests/hsw-exclusion-audit-receipt.log").read_text()
        cls.handoff = Path("build/tests/hsw-handoff-audit-receipt.log").read_text()

    def result(self, text):
        wake = forcewake_result(text)
        self.assertIsNotNone(wake)
        self.assertEqual(wake["status"], "VERIFIED")
        return wake["engine_drain"]

    def test_both_held_samples_are_complete(self):
        drain = self.result(self.text)
        self.assertEqual((drain["maps"], drain["retired"], drain["reads"]), (4, 4, 40))
        self.assertEqual(set(drain["engines"].values()), {"idle-at-samples"})
        self.assertEqual(drain["requesters"], {"igd": "clear-at-samples", "display-hda": "unavailable"})
        self.assertEqual(drain["requester_drain"], "unverified")
        timing = drain["timing"]
        self.assertTrue(timing["entered"])
        self.assertTrue(timing["irq_enabled_before"])
        self.assertEqual(timing["timer_ticks"], [63, 63])
        self.assertEqual(timing["preemptions"], [31, 31])
        self.assertEqual(timing["stop"], "complete")

    def test_busy_changed_and_unavailable_are_preserved(self):
        for value, state in ((0, "idle-unconfirmed"), (4294967295, "unavailable")):
            text = re.sub(r"(engine=0 head=256 tail=256 ctl=1 mode=)512", rf"\g<1>{value}", self.text)
            self.assertEqual(self.result(text)["engines"]["render"], state)
        busy = self.text.replace("engine=0 head=256", "engine=0 head=240")
        self.assertEqual(self.result(busy)["engines"]["render"], "queued-or-waiting")
        changed = self.text.replace("phase=1 engine=0 head=256", "phase=1 engine=0 head=240")
        self.assertEqual(self.result(changed)["engines"]["render"], "changed")
        for pattern, replacement in ((r"(requester=0 [^\n]* af=136,\d+,)0", r"\g<1>256"),
                                     (r"(requester=0 [^\n]* pcie=96 status=)0", r"\g<1>32")):
            pending = re.sub(pattern, replacement, self.text, count=1)
            self.assertNotEqual(pending, self.text)
            self.assertEqual(self.result(pending)["requesters"]["igd"], "pending-seen")

    def test_held_read_evidence_cannot_be_skipped(self):
        for old, new in (
            ("samples=2 maps=4", "samples=1 maps=4"),
            ("maps=4 retired=4", "maps=4 retired=3"),
            ("ack=1,1", "ack=1,0"),
            ("thread=0,0", "thread=0,3"),
            ("ring_reads=40", "ring_reads=39"),
            ("phase=1 engine=3", "phase=1 engine=2"),
            ("authority=none", "authority=owned"),
            ("ticks=63,63", "ticks=63,64"),
            ("stop=complete", "stop=sample-deadline"),
            (ENGINE_DRAIN_TOKEN, "srvros.hsw-forcewake=acquire-release"),
        ):
            with self.subTest(new=new):
                self.assertIn(old, self.text)
                self.assertIsNone(forcewake_result(self.text.replace(old, new, 1)))

    def test_staging_is_exclusive_and_requires_automatic_return(self):
        version = "ram-usb-v12-hsw-laptop-probe-6ada85bb"
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config("1234567890abcdef", version)
        config = boot.limine_config("1234567890abcdef", version + "-auto-return")
        self.assertIn(ENGINE_DRAIN_TOKEN, config)
        self.assertNotIn("srvros.hsw-forcewake=acquire-release", config)
        with self.assertRaisesRegex(ValueError, "isolated automatic return"):
            boot.stage(haswell_forcewake=True, haswell_engine_drain=True)

    def test_smi_route_observations_do_not_grant_drain_or_exclusion(self):
        audit = forcewake_result(self.audit)["exclusion_audit"]
        self.assertEqual(audit["pch_smi_route"], "enabled-observed")
        self.assertEqual((audit["firmware_exclusion"], audit["requester_drain"]), ("unverified", "unverified"))
        self.assertTrue(all(endpoint["pch_global_smi_locked"] for endpoint in audit["samples"]))
        disabled = forcewake_result(self.audit.replace("pmcon=16 en=33", "pmcon=16 en=32"))["exclusion_audit"]
        self.assertEqual(disabled["pch_smi_route"], "disabled-at-samples")
        self.assertEqual(disabled["firmware_exclusion"], "unverified")
        unavailable_text = self.audit.replace("read=1 valid=1 pmbase=6145 acpi=128 pmcon=16 en=33 sts=32",
                                              "read=0 valid=0 pmbase=8193 acpi=128 pmcon=16 en=0 sts=0")
        self.assertEqual(forcewake_result(unavailable_text)["exclusion_audit"]["pch_smi_route"], "unavailable")

    def test_smi_receipt_rejects_unbound_route_and_authority(self):
        for old, new in (
            ("rows=2 firmware_exclusion=unverified", "rows=1 firmware_exclusion=unverified"),
            ("pmbase=6145", "pmbase=8193"),
            ("acpi=128", "acpi=0"),
            ("en=33 sts=32", "en=4294967295 sts=32"),
            ("read=1 valid=1", "read=0 valid=1"),
            ("firmware_exclusion=unverified", "firmware_exclusion=verified"),
            (EXCLUSION_AUDIT_TOKEN, ENGINE_DRAIN_TOKEN),
        ):
            with self.subTest(new=new):
                self.assertIn(old, self.audit)
                self.assertIsNone(forcewake_result(self.audit.replace(old, new, 1)))

    def test_exclusion_audit_recipe(self):
        version = "ram-usb-v13-hsw-laptop-probe-6ada85bb"
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config("1234567890abcdef", version)
        config = boot.limine_config("1234567890abcdef", version + "-auto-return")
        self.assertIn(EXCLUSION_AUDIT_TOKEN, config)
        self.assertNotIn(ENGINE_DRAIN_TOKEN, config)
        with self.assertRaisesRegex(ValueError, "isolated automatic return"):
            boot.stage(haswell_engine_drain=True, haswell_exclusion_audit=True)

    def test_handoff_audit_reports_interface_without_authority(self):
        audit = forcewake_result(self.handoff)["handoff_audit"]
        self.assertEqual(audit["opregion_state"], "present-at-samples")
        self.assertEqual((audit["requester_hold"], audit["firmware_handoff"]), ("unavailable", "unavailable"))
        self.assertTrue(all(r["asls"] == 0x8cd18190 and r["carrier"] == "acpi-nvs" for r in audit["samples"]))
        self.assertTrue(all(r["sci_selected"] and not r["trigger_set"] for r in audit["samples"]))
        changed = self.handoff.replace("swsci=32768 carrier=1", "swsci=32769 carrier=1", 1)
        self.assertEqual(forcewake_result(changed)["handoff_audit"]["opregion_state"], "changed")
        absent = self.handoff.replace(f"asls={0x8cd18190} swsci=32768 carrier=1", "asls=0 swsci=0 carrier=0")
        self.assertEqual(forcewake_result(absent)["handoff_audit"]["opregion_state"], "absent-at-samples")

    def test_handoff_receipt_rejects_unbound_carriers_and_grants(self):
        for old, new in (
            ("rows=2 requester_hold=unavailable", "rows=1 requester_hold=unavailable"),
            ("firmware_handoff=unavailable", "firmware_handoff=verified"),
            ("authority=none", "authority=owned"),
            ("stable=1 asls=", "stable=0 asls="),
            (f"asls={0x8cd18190}", "asls=0"),
            (f"asls={0x8cd18190}", "asls=4294967292"),
            (HANDOFF_AUDIT_TOKEN, EXCLUSION_AUDIT_TOKEN),
        ):
            with self.subTest(new=new):
                self.assertIn(old, self.handoff)
                self.assertIsNone(forcewake_result(self.handoff.replace(old, new, 1)))

    def test_handoff_audit_recipe(self):
        version = "ram-usb-v14-hsw-laptop-probe-6ada85bb"
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config("1234567890abcdef", version)
        config = boot.limine_config("1234567890abcdef", version + "-auto-return")
        self.assertIn(HANDOFF_AUDIT_TOKEN, config)
        self.assertNotIn(EXCLUSION_AUDIT_TOKEN, config)
        with self.assertRaisesRegex(ValueError, "isolated automatic return"):
            boot.stage(haswell_handoff_audit=True, haswell_exclusion_audit=True)


if __name__ == "__main__":
    unittest.main()
