import re
import subprocess
from pathlib import Path
import unittest
from unittest.mock import patch

import hardware_boot as boot
from hardware_haswell_live_vtd import live_result
from hardware_air61_read_qualification import DRAIN_OFFSETS
from hardware_air61_register_sample import DRAIN_TOKEN
from hardware_haswell_candidate import candidate_result
from haswell_vtd_capability_audit import OFFSETS
from test_hardware_haswell_ecam import SESSION

RELEASE = "a" * 16


class LiveReceipt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        exe = Path("build/tests/hsw-backing-adapter")
        cls.capture = subprocess.check_output([exe, "live"], text=True)
        profile = "ram-usb-v9-hfw-laptop-probe-6ada85bb-auto-return"
        cmdline = next(line.split("cmdline: ", 1)[1] for line in
                       boot.limine_config(RELEASE, profile).splitlines() if "cmdline: " in line)
        cls.exit_log = "laptop-capture: boot options=" + cmdline + "\n" + cls.capture
        before, after = (tuple(map(int, row)) for row in re.findall(
            r"hsw-live-requester: phase=[01] begin=(\d+) end=(\d+)", cls.capture))
        values = (0, 0x10, 0, 0x0c02, 0, 0, 0, 0, 0, 0, 0, 0)
        raw = list(zip(OFFSETS, values))
        cls.proof = dict(status="OBSERVED_RETIRED_READ_ONLY_SAMPLE", retirement_verified=True,
                         claim_released=True, begin=before[1]+1, end=after[0]-1, raw_samples=[raw, raw])
        cls.access = dict(status="OBSERVED", candidate=candidate_result(cls.capture),
                          declared_rmrrs_covering_target=[
                              dict(segment=0, scopes=[dict(kind=1, bus=0, path=[(2, 0)])])])

    def result(self, text=None, **changes):
        # The actual register/cache/graph and slot/DMAR readers have their
        # own positive producer fixtures. Exercise the join against the
        # actual C requester producer with independent accepted inputs.
        proof = dict(self.proof, **changes)
        with patch("hardware_haswell_live_vtd.sample_result", return_value=proof), \
             patch("hardware_haswell_live_vtd.access_result", return_value=self.access):
            return live_result("window", "ecam", text or self.exit_log, SESSION, RELEASE)

    def test_live_observation_keeps_engine_ownership_unknown(self):
        proof = self.result()
        self.assertEqual(proof["status"], "OBSERVED")
        self.assertFalse(proof["remapper_sample"]["sampled_status"]["translation_enabled"])
        self.assertTrue(proof["remapper_sample"]["root_address_zero"])
        self.assertTrue(proof["target_in_igd_rmrr"])
        self.assertEqual(len(proof["gt"]), 2)
        self.assertTrue(proof["requesters"][0]["bus_master"])
        self.assertFalse(proof["hardware_authority"])

    def test_af_and_same_cpu_smi_wrap_are_observations(self):
        observation = self.result()["requester_observation"]
        self.assertEqual(observation["smi"], [0xffffffff, 0])
        self.assertEqual(observation["smi_delta"], 1)
        self.assertTrue(observation["af"][0]["igd"]["transactions_pending"])
        self.assertIsNone(observation["af"][0]["hda"]["transactions_pending"])
        self.assertFalse(observation["requester_drain_proven"])
        self.assertFalse(observation["firmware_exclusion_proven"])
        without = "\n".join(row for row in self.exit_log.splitlines()
                            if not row.startswith(("hsw-live-af:", "hsw-live-observe:")))
        self.assertIsNone(self.result(without)["requester_observation"])
        for line in self.exit_log.splitlines():
            if line.startswith(("hsw-live-af:", "hsw-live-observe:")):
                self.assertIsNone(self.result(self.exit_log.replace(line+"\n", "")))
        self.assertIsNone(self.result(self.exit_log.replace("smi=4294967295,0", "smi=4294967296,0")))
        self.assertIsNone(self.result(self.exit_log.replace("hda=0,0,0", "hda=0,0,1")))

    def test_rejects_cross_release_drift_or_out_of_order(self):
        for old, new in (("cmd=7 pm=0", "cmd=3 pm=0"), ("gt_rows=2", "gt_rows=1"),
                         ("desktop-release="+RELEASE, "desktop-release="+("b"*16)),
                         ("writes=0 authority=none", "writes=1 authority=none")):
            self.assertIsNone(self.result(self.exit_log.replace(old, new)))
        for changes in (dict(begin=1), dict(end=2**63), dict(retirement_verified=False),
                        dict(claim_released=False), dict(status="REFUSED")):
            self.assertIsNone(self.result(**changes))

    def test_drain_state_join_keeps_a_nonzero_fault_observational(self):
        opts = self.exit_log.replace('srvros.hsw-live-vtd=read-only', 'srvros.hsw-live-vtd=read-only '+DRAIN_TOKEN)
        raw = list(zip(DRAIN_OFFSETS, (0,0x10,0x00f0101a,0x08000000,0,0x08000000,2,0x02000000,0,0x02000000,2,0)))
        proof = self.result(opts, raw_samples=[raw,raw])
        self.assertTrue(proof['drain_state_profile'])
        self.assertFalse(proof['remapper_sample']['fault_status_clear'])
        self.assertFalse(proof['remapper_sample']['requester_drain_proven'])
        self.assertIsNone(self.result(self.exit_log, raw_samples=[raw,raw]))

    def test_release_config_is_distinct_explicit_and_automatic(self):
        profile = "ram-usb-v9-hfw-laptop-probe-6ada85bb"
        cmd = boot.limine_config(RELEASE, profile+"-auto-return")
        for token in ("srvros.hsw-live-vtd=read-only", "srvros.vtd-window=air61-two-pass-read-only",
                      "srvros.ecam=air61-2m-test", "srvros.capture-failure=reboot"):
            self.assertEqual(cmd.split().count(token), 1)
        self.assertNotIn("srvros.hsw-live-vtd", boot.limine_config(RELEASE, profile.replace("v9", "v8")))
        with self.assertRaisesRegex(ValueError, "automatic return"):
            boot.limine_config(RELEASE, profile)
        with self.assertRaisesRegex(ValueError, "guarded register sampler"):
            boot.stage(haswell_live_vtd=True)
        drain = profile.replace('v9','v10')+'-auto-return'
        self.assertEqual(boot.limine_config(RELEASE,drain).split().count(DRAIN_TOKEN),1)
        self.assertNotIn(DRAIN_TOKEN,cmd)
        with self.assertRaisesRegex(ValueError, "isolated live VT-d profile"):
            boot.stage(haswell_drain_state=True)
