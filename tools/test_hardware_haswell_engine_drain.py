import re
import unittest
from pathlib import Path

import hardware_boot as boot
from hardware_haswell_engine_drain import ENGINE_DRAIN_TOKEN
from hardware_haswell_forcewake import forcewake_result


class HaswellEngineDrainReceipt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = Path("build/tests/hsw-engine-drain-receipt.log").read_text()

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


if __name__ == "__main__":
    unittest.main()
