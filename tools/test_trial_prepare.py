#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import trial_prepare as prep
from test_trial import trial as collect_cli


class PrepareReturnTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.run_root = self.root / "runs"
        self.q = self.root / "qualification.json"
        self.previous = "1122334455667788"
        self.volume = "abcd0123"
        self.version = "ram-usb-v1-retina-desktop-" + self.volume
        hashes = {name: hashlib.sha256(name.encode()).hexdigest() for name in prep.boot.FILES}
        self.release = prep.boot.release_id({k: v for k, v in hashes.items() if not k.endswith("limine.conf")}, self.version)
        self.manifest = dict(release=self.release, config_version=self.version, sha256=hashes, git_head="qualified-checkout")
        digest = hashlib.sha256((json.dumps(self.manifest, indent=2) + "\n").encode()).hexdigest()
        stage = dict(release=self.release, manifest=self.manifest, manifest_sha256=digest)
        prep.write_json(self.root / "stage.json", stage)
        cases = {}
        for case in prep.CASES:
            vm = dict(status="PASS", release=self.release, manifest_sha256=digest)
            if case == "present":
                vm.update(retina_desktop=True, retina_fractional=True, workspace_table="gpt", saved=True,
                          unmounted=True, automatic_return=True)
            else:
                vm.update(retina_missing_workspace=True, returned=True)
            path = self.root / case / prep.RESULT
            path.parent.mkdir(parents=True)
            prep.write_json(path, vm)
            cases[case] = vm
        self.serial = self.root / "present/build/review/hardware-grub-serial.log"
        self.serial.write_bytes(b"deskhelp: page=retina-trial\r\n")
        prep.write_json(self.q, dict(stage, format="srvros-trial-qualification-v1", recipe="retina",
                        status="passed", workspace_uuid=self.volume, source_checkout=str(self.root), cases=cases))
        work = self.root / "mount/srvros-work"
        work.mkdir(parents=True)
        (work / ".workspace").write_text("SRVROS-WORKSPACE-V1 abcd0123\n")
        self.work = work

    def prepare(self, *, mismatch=False):
        baseline = prep.workspace_baseline(self.work.parent, self.volume)
        installed = dict(self.manifest, git_head="installed-checkout")
        if mismatch:
            installed["sha256"] = dict(installed["sha256"], **{"limine.conf": "0" * 64})
        sample = dict(model="MacBookPro11,2", prior_boot_id="before", installed=installed,
                      recovery_manifest_sha256="f"*64, baseline=baseline)
        with patch.object(prep, "inspect_host", return_value=sample):
            return prep.prepare(self.q, self.previous, self.root / "archive", self.run_root, False)

    def test_saved_bundle_collects_new_release_session(self):
        old = "session-0123456789abcdef-00"
        (self.work / "Logs" / old).mkdir(parents=True)
        path, run = self.prepare()
        self.assertEqual(run["phase"], "verified_installed_unarmed")
        self.assertNotEqual(run["source"], run["qualification_source"])
        baseline, prior = collect_cli.prior_sessions(path.parent, run, self.volume)
        self.assertEqual(prior, {old})
        self.assertEqual(collect_cli.workspace_changes(self.work, baseline)["changed"], [])
        new = "session-abcdef0123456789-00"
        logs = path.parent / "return-evidence/sessions" / new
        logs.mkdir(parents=True)
        (logs / "boot.log").write_text(f"cmdline: srvros.desktop-release={self.release}\n")
        from test_trial import framed
        (logs / "exit.log").write_bytes(framed("exit.log", b"retina-scales: applied=30 kept=24 reverted=2 timed_out=4\n"))
        result, _, complete = collect_cli.collect(path.parent, True)
        self.assertTrue(complete)
        self.assertEqual(result["session"], new)

    def test_repeat_keeps_prior_file_and_allocates_new_checklist_name(self):
        proof = self.work / prep.PROOF.upper()
        proof.write_text("prior result")
        (self.work / "retina-fractional-proof-2.txt").mkdir()
        _, run = self.prepare()
        self.assertEqual(run["phase"], "checklist_handoff_required")
        self.assertEqual(run["proof_file"], "retina-fractional-proof-3.txt")
        self.assertIn(run["proof_file"], " ".join(run["checklist"]))
        self.assertEqual(proof.read_text(), "prior result")

    def test_wrong_installed_bytes_or_missing_checklist(self):
        with self.assertRaisesRegex(ValueError, "Installed bytes differ"):
            self.prepare(mismatch=True)
        self.serial.write_text("echo deskhelp: page=retina-trial\n")
        with self.assertRaisesRegex(ValueError, "opening Retina Help"):
            self.prepare()
        self.assertFalse(self.run_root.exists())

    def test_wrong_case_or_manifest(self):
        path = self.root / "missing" / prep.RESULT
        vm = json.loads(path.read_bytes())
        prep.write_json(path, dict(vm, release=self.previous))
        with self.assertRaisesRegex(ValueError, "mismatched release"):
            prep.qualification(self.q)
        for path in [self.root / "stage.json", self.q]:
            obj = json.loads(path.read_bytes())
            obj["manifest"]["git_head"] = "altered"
            prep.write_json(path, obj)
        with self.assertRaisesRegex(ValueError, "digest mismatch"):
            prep.qualification(self.q)

    def test_pending_boot_selections_refused(self):
        cfg = prep.boot.grub_config("ABCD-1234", self.release, self.previous)
        for env, firmware, expected in [("next_entry=other", "", "GRUB already"), ("", "BootNext: 0000", "BootNext")]:
            with self.subTest(env=env), patch.object(prep.host, "check_ubuntu_default"), \
                 patch.object(Path, "read_text", return_value=cfg), \
                 patch.object(subprocess, "check_output", side_effect=["ABCD-1234", env, firmware]):
                with self.assertRaisesRegex(ValueError, expected):
                    prep.boot_selection(self.release, self.previous)

    def test_recovery_must_match_the_known_good_release_and_archive(self):
        state = self.root / "state.json"
        prep.write_json(state, dict(current=self.release, previous=self.previous))
        archive = self.root / "archive"
        archive.mkdir()
        class Device:
            def __init__(self, number):
                self.st_dev = number
        def stat_device(path):
            return Device(2 if path == archive else 1)
        with patch.object(prep.boot, "BASE", self.root), patch.object(Path, "is_mount", return_value=True), \
             patch.object(Path, "stat", stat_device), patch.object(prep.boot, "verify_files", side_effect=[
                 self.manifest, {"release": self.previous}, {"release": self.release}]):
            with self.assertRaisesRegex(ValueError, "archived copy"):
                prep.inspect_payloads(self.release, self.previous, archive)
            with self.assertRaisesRegex(ValueError, "release pair"):
                prep.inspect_payloads(self.release, "0000000000000001", archive)

    def test_private_mount_is_released_on_inspection_error(self):
        with patch.object(subprocess, "check_output", side_effect=['{}', 'gpt']), \
             patch.object(prep, "select_partition", return_value="/dev/fixture"), patch.object(Path, "is_block_device", return_value=True), \
             patch.object(prep, "check_clean_image"), patch.object(subprocess, "run") as commands, \
             patch.object(prep, "workspace_baseline", side_effect=ValueError("bad marker")):
            with self.assertRaisesRegex(ValueError, "bad marker"):
                prep.inspect_workspace(self.volume)
            mount, unmount = [c.args[0] for c in commands.call_args_list]
            self.assertIn("ro,nosuid,nodev,noexec", mount)
            self.assertEqual(unmount, ["umount", mount[-1]])


if __name__ == "__main__":
    unittest.main()
